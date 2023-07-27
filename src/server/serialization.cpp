//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "serialization.hpp"

#include <boost/describe/class.hpp>
#include <boost/json/array.hpp>
#include <boost/json/fwd.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/redis/resp3/type.hpp>

#include <chrono>
#include <string>
#include <string_view>

#include "error.hpp"

namespace resp3 = boost::redis::resp3;

namespace chat {

// User wire format
struct wire_user
{
    std::string id;
    std::string username;
};
BOOST_DESCRIBE_STRUCT(wire_user, (), (id, username))

// Message as received from the client; no timestamp or ID
struct client_message_wire
{
    wire_user user;
    std::string content;
};
BOOST_DESCRIBE_STRUCT(client_message_wire, (), (user, content))

// Message as stored in Redis, no ID
struct redis_message_wire
{
    wire_user user;
    std::string content;
    std::int64_t timestamp;
};
BOOST_DESCRIBE_STRUCT(redis_message_wire, (), (user, content, timestamp))

// messages event wire format
struct messages_event_wire
{
    std::string roomId;
    std::vector<client_message_wire> messages;
};
BOOST_DESCRIBE_STRUCT(messages_event_wire, (), (roomId, messages))

// requestRoomHistory event wire format
struct request_room_history_wire
{
    std::string roomId;
    std::string firstMessageId;
};
BOOST_DESCRIBE_STRUCT(request_room_history_wire, (), (roomId, firstMessageId))

}  // namespace chat

static std::int64_t serialize_datetime(chat::timestamp_t input) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(input.time_since_epoch()).count();
}

static chat::timestamp_t parse_datetime(std::int64_t input) noexcept
{
    return chat::timestamp_t(std::chrono::milliseconds(input));
}

static chat::message to_message(chat::redis_message_wire&& from, std::string_view id)
{
    return chat::message{
        std::string(id),
        std::move(from.content),
        chat::user{
                   std::move(from.user.id),
                   std::move(from.user.username),
                   },
        parse_datetime(from.timestamp)
    };
}

static chat::message to_message(chat::client_message_wire&& from)
{
    return chat::message{
        "", // no ID
        std::move(from.content),
        chat::user{
                   std::move(from.user.id),
                   std::move(from.user.username),
                   },
        {}  // no timestamp
    };
}

static boost::json::array serialize_messages(boost::span<const chat::message> messages)
{
    boost::json::array res;
    res.reserve(messages.size());
    for (const auto& msg : messages)
    {
        res.push_back(boost::json::object({
            {"id",        msg.id                           },
            {"content",   msg.content                      },
            {"user",
             {
                 {"id", msg.usr.id},
                 {"username", msg.usr.username},
             }                                             },
            {"timestamp", serialize_datetime(msg.timestamp)},
        }));
    }
    return res;
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
            auto msg = boost::json::try_value_to<chat::redis_message_wire>(jv);
            if (msg.has_error())
                CHAT_RETURN_ERROR(msg.error())
            res.back().push_back(to_message(std::move(msg.value()), *data.id));
            data.state = wants_level0_or_entry_list;
            data.id = nullptr;
        }
    }

    if (data.state != wants_level0_or_entry_list)
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
    chat::redis_message_wire redis_msg{
        wire_user{msg.usr.id, msg.usr.username},
        msg.content,
        serialize_datetime(msg.timestamp),
    };
    return boost::json::serialize(boost::json::value_from(redis_msg));
}

std::string chat::serialize_hello_event(const hello_event& evt)
{
    // Rooms
    boost::json::array json_rooms;
    json_rooms.reserve(evt.rooms.size());
    for (const auto& room : evt.rooms)
    {
        json_rooms.push_back(boost::json::object({
            {"id",              room.id                          },
            {"name",            room.name                        },
            {"messages",        serialize_messages(room.messages)},
            {"hasMoreMessages", room.has_more_messages           },
        }));
    }

    // Event
    // clang-format off
    boost::json::value res{
        {"type",    "hello"},
        {"payload", {
            {"rooms", std::move(json_rooms)},
        }}
    };
    // clang-format on
    return boost::json::serialize(res);
}

std::string chat::serialize_messages_event(const messages_event& evt)
{
    // clang-format off
    boost::json::value res{
        {"type",    "messages" },
        {"payload", {
            {"roomId", evt.room_id},
            {"messages", serialize_messages(evt.messages)}},
        }
    };
    // clang-format on
    return boost::json::serialize(res);
}

std::string chat::serialize_room_history_event(const room_history_event& evt)
{
    // clang-format off
    boost::json::value res{
        {"type",    "roomHistory" },
        {"payload", {
            {"roomId", evt.room_id},
            {"messages", serialize_messages(evt.messages)}},
            {"hasMoreMessages", evt.has_more_messages},
        }
    };
    // clang-format on
    return boost::json::serialize(res);
}

chat::any_client_event chat::parse_client_event(std::string_view from)
{
    error_code ec;

    // Parse the JSON
    auto msg = boost::json::parse(from, ec);
    if (ec)
        return ec;

    // Get the message type
    const auto* obj = msg.if_object();
    if (!obj)
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    auto it = obj->find("type");
    if (it == obj->end())
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    const auto& type = it->value();

    // Get the payload
    it = obj->find("payload");
    if (it == obj->end())
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    const auto& payload = it->value();

    if (type == "messages")
    {
        // Parse the payload
        auto parsed_payload = boost::json::try_value_to<chat::messages_event_wire>(payload);
        if (parsed_payload.has_error())
            CHAT_RETURN_ERROR(parsed_payload.error())
        auto& evt = parsed_payload.value();

        // Compose the event
        chat::messages_event res{std::move(evt.roomId)};
        res.messages.reserve(evt.messages.size());
        for (auto& msg : evt.messages)
        {
            res.messages.push_back(to_message(std::move(msg)));
        }

        return res;
    }
    else if (type == "requestRoomHistory")
    {
        // Parse the payload
        auto parsed_payload = boost::json::try_value_to<chat::request_room_history_wire>(payload);
        if (parsed_payload.has_error())
            CHAT_RETURN_ERROR(parsed_payload.error())
        auto& evt = parsed_payload.value();

        // Compose the event
        return chat::request_room_history_event{
            std::move(evt.roomId),
            std::move(evt.firstMessageId),
        };
    }
    else
    {
        // Unknown typpe
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    }
}
