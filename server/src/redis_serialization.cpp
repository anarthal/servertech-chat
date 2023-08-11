//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "redis_serialization.hpp"

#include <boost/describe/class.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/redis/resp3/type.hpp>

#include <string>

#include "error.hpp"
#include "timestamp.hpp"

namespace resp3 = boost::redis::resp3;
using namespace chat;

namespace chat {

// User as stored in Redis
struct redis_wire_user
{
    std::string id;
    std::string username;
};
BOOST_DESCRIBE_STRUCT(redis_wire_user, (), (id, username))

// Message as stored in Redis, no ID
struct redis_wire_message
{
    redis_wire_user user;
    std::string content;
    std::int64_t timestamp;
};
BOOST_DESCRIBE_STRUCT(redis_wire_message, (), (user, content, timestamp))

}  // namespace chat

static chat::message to_message(chat::redis_wire_message&& from, std::string_view id)
{
    return chat::message{
        std::string(id),
        std::move(from.content),
        chat::user{
                   std::move(from.user.id),
                   std::move(from.user.username),
                   },
        parse_timestamp(from.timestamp)
    };
}

chat::result<std::vector<std::vector<chat::message>>> chat::parse_room_history_batch(
    const boost::redis::generic_response& from
)
{
    std::vector<std::vector<chat::message>> res;
    error_code ec;

    // Verify success
    if (from.has_error())
        CHAT_RETURN_ERROR(errc::redis_parse_error)  // TODO: log diagnostics
    const auto& nodes = from.value();

    // We need a one-pass parser. Every response has the following format:
    // list of MessageEntry:
    //    MessageEntry[0]: string (id)
    //    MessageEntry[1]: list<string> (key-value pairs; always an even number)
    // Since manipulating nodes is cumbersome, we have a single key named "payload",
    // with a single value containing a JSON
    // This function is capable of parsing multiple, batched responses
    enum state_t
    {
        wants_level0_list,
        wants_level0_or_entry_list,
        wants_id,
        wants_attr_list,
        wants_key,
        wants_value
    };

    struct parser_data_t
    {
        state_t state{wants_level0_list};
        const std::string* id{};
    } data;

    for (const auto& node : nodes)
    {
        if (data.state == wants_level0_list)
        {
            if (node.data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 0u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            res.emplace_back();
            data.state = wants_level0_or_entry_list;
        }
        else if (data.state == wants_level0_or_entry_list)
        {
            if (node.data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth == 0u)
            {
                res.emplace_back();
                data.state = wants_level0_or_entry_list;
            }
            else if (node.depth == 1u)
            {
                if (node.aggregate_size != 2u)
                    CHAT_RETURN_ERROR(errc::redis_parse_error)
                data.state = wants_id;
            }
            else
            {
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            }
        }
        else if (data.state == wants_id)
        {
            if (node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 2u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.id = &node.value;
            data.state = wants_attr_list;
        }
        else if (data.state == wants_attr_list)
        {
            if (node.data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 2u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.aggregate_size != 2u)  // single key/value pair, serialized as JSON
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.state = wants_key;
        }
        else if (data.state == wants_key)
        {
            if (node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 3u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.value != "payload")
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.state = wants_value;
        }
        else if (data.state == wants_value)
        {
            if (node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 3u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)

            // Parse payload
            auto jv = boost::json::parse(node.value, ec);
            if (ec)
                CHAT_RETURN_ERROR(ec)
            auto msg = boost::json::try_value_to<chat::redis_wire_message>(jv);
            if (msg.has_error())
                CHAT_RETURN_ERROR(msg.error())
            res.back().push_back(to_message(std::move(msg.value()), *data.id));
            data.state = wants_level0_or_entry_list;
            data.id = nullptr;
        }
    }

    if (data.state != wants_level0_or_entry_list && data.state != wants_level0_list)
        CHAT_RETURN_ERROR(errc::redis_parse_error)

    return res;
}

chat::result<std::vector<chat::message>> chat::parse_room_history(const boost::redis::generic_response& from)
{
    auto res = parse_room_history_batch(from);
    if (res.has_error())
        return res.error();
    return std::move(res->at(0));
}

chat::result<std::vector<std::string>> chat::parse_string_list(const boost::redis::generic_response& from)
{
    if (from.has_error())
        CHAT_RETURN_ERROR(errc::redis_parse_error)  // TODO: log diagnostics
    const auto& nodes = from.value();

    std::vector<std::string> res;
    res.reserve(nodes.size());
    for (const auto& node : nodes)
    {
        if (node.depth != 0u)
            CHAT_RETURN_ERROR(errc::redis_parse_error)
        else if (node.data_type != resp3::type::blob_string)
            CHAT_RETURN_ERROR(errc::redis_parse_error)
        res.push_back(node.value);
    }
    return res;
}

std::string chat::serialize_redis_message(const message& msg)
{
    chat::redis_wire_message redis_msg{
        redis_wire_user{msg.usr.id, msg.usr.username},
        msg.content,
        serialize_timestamp(msg.timestamp),
    };
    return boost::json::serialize(boost::json::value_from(redis_msg));
}
