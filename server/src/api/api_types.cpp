//
// Copyright (c) 2023-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "api/api_types.hpp"

#include <boost/describe/class.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/variant2/variant.hpp>

#include <cstdint>
#include <string_view>

#include "business_types.hpp"
#include "timestamp.hpp"

using namespace chat;

namespace chat {

//
// BOOST_DESCRIBE_STRUCT is used to add reflection capabilities to structs.
// It's used by boost::json::value_to, value_from and try_value_from to
// automatically generate JSON parsing/serializing code.
//
// Describe metadata is defined in this .cpp file to reduce build times.
// Care must be taken to not redefine this metadata in other files, which is
// an ODR violation.
//
// We only need such metadata on incoming types, since these are an exact
// representation of the wire format used by the client.
//

BOOST_DESCRIBE_STRUCT(create_account_request, (), (username, email, password))
BOOST_DESCRIBE_STRUCT(login_request, (), (email, password))
BOOST_DESCRIBE_STRUCT(client_message, (), (content))
BOOST_DESCRIBE_STRUCT(client_messages_event, (), (roomId, messages))
BOOST_DESCRIBE_STRUCT(request_room_history_event, (), (roomId, firstMessageId))

}  // namespace chat

namespace {

// We also define some helper structs with Describe metadata for outgoing
// types. This makes serialization code easier.

// API error wire format
struct wire_api_error
{
    std::string_view id;
    std::string_view message;
};
BOOST_DESCRIBE_STRUCT(wire_api_error, (), (id, message))

// User wire format
struct wire_user
{
    std::int64_t id;
    std::string_view username;
};
BOOST_DESCRIBE_STRUCT(wire_user, (), (id, username))

// Server message wire format
struct wire_server_message
{
    std::string_view id;
    std::string_view content;
    wire_user user;
    std::int64_t timestamp;
};
BOOST_DESCRIBE_STRUCT(wire_server_message, (), (id, content, user, timestamp))

}  // namespace

//
// Incoming types (HTTP requests, websocket client events)
//

// Helper for HTTP requests
template <class RequestType>
static result<RequestType> parse_generic_request(std::string_view from)
{
    // Parse the JSON
    error_code ec;
    auto msg = boost::json::parse(from, ec);
    if (ec)
        CHAT_RETURN_ERROR(ec)

    // Parse into the struct
    return boost::json::try_value_to<RequestType>(msg);
}

result<create_account_request> create_account_request::from_json(std::string_view from)
{
    return parse_generic_request<create_account_request>(from);
}

result<login_request> login_request::from_json(std::string_view from)
{
    return parse_generic_request<login_request>(from);
}

chat::any_client_event chat::parse_client_event(std::string_view from)
{
    error_code ec;

    // Parse the JSON
    auto msg = boost::json::parse(from, ec);
    if (ec)
        CHAT_RETURN_ERROR(ec)

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

    // Parse the message, depending on its type
    if (type == "clientMessages")
    {
        // Parse the payload
        auto parsed_payload = boost::json::try_value_to<client_messages_event>(payload);
        if (parsed_payload.has_error())
            CHAT_RETURN_ERROR(parsed_payload.error())
        return parsed_payload.value();
    }
    else if (type == "requestRoomHistory")
    {
        // Parse the payload
        auto parsed_payload = boost::json::try_value_to<request_room_history_event>(payload);
        if (parsed_payload.has_error())
            CHAT_RETURN_ERROR(parsed_payload.error())
        return parsed_payload.value();
    }
    else
    {
        // Unknown typpe
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    }
}

//
// Outgoing types (HTTP responses, websocket server events)
//

static std::string_view to_string(api_error_id input)
{
    switch (input)
    {
    case api_error_id::login_failed: return "LOGIN_FAILED";
    case api_error_id::username_exists: return "USERNAME_EXISTS";
    case api_error_id::email_exists: return "EMAIL_EXISTS";
    case api_error_id::bad_request:
    default: return "BAD_REQUEST";
    }
}

std::string api_error::to_json() const
{
    wire_api_error err{to_string(error_id), error_message};
    return boost::json::serialize(boost::json::value_from(err));
}

static boost::json::value serialize_message(const message& input, std::string_view username)
{
    return boost::json::value_from(wire_server_message{
        input.id,
        input.content,
        wire_user{input.user_id, username},
        serialize_timestamp(input.timestamp),
    });
}

static boost::json::array serialize_messages(
    boost::span<const message> messages,
    const username_map& usernames
)
{
    boost::json::array res;
    res.reserve(messages.size());
    for (const auto& msg : messages)
    {
        // Lookup the username in the map. Default to empty if not found
        auto it = usernames.find(msg.user_id);
        auto username = it == usernames.end() ? std::string_view() : std::string_view(it->second);

        res.push_back(serialize_message(msg, username));
    }
    return res;
}

static boost::json::array serialize_messages(boost::span<const message> messages, const user& sending_user)
{
    boost::json::array res;
    res.reserve(messages.size());
    for (const auto& msg : messages)
    {
        assert(msg.user_id == sending_user.id);
        res.push_back(serialize_message(msg, sending_user.username));
    }
    return res;
}

static boost::json::object serialize_room(const room& input, const username_map& usernames)
{
    boost::json::object res({
        {"id",              input.id              },
        {"name",            input.name            },
        {"hasMoreMessages", input.history.has_more},
    });
    res.emplace("messages", serialize_messages(input.history.messages, usernames));
    return res;
}

static std::string serialize_event(std::string_view type, boost::json::object payload)
{
    boost::json::object evt;
    evt.emplace("type", type);
    evt.emplace("payload", std::move(payload));
    return boost::json::serialize(evt);
}

std::string hello_event::to_json() const
{
    // Current user
    auto json_me = boost::json::value_from(wire_user{me.id, me.username});

    // Rooms
    boost::json::array json_rooms;
    json_rooms.reserve(rooms.size());
    for (const auto& room : rooms)
        json_rooms.push_back(serialize_room(room, usernames));

    // Event
    boost::json::object payload;
    payload.emplace("me", std::move(json_me));
    payload.emplace("rooms", std::move(json_rooms));
    return serialize_event("hello", std::move(payload));
}

std::string server_messages_event::to_json() const
{
    boost::json::object payload;
    payload.emplace("roomId", room_id);
    payload.emplace("messages", serialize_messages(messages, sending_user));
    return serialize_event("serverMessages", std::move(payload));
}

std::string room_history_event::to_json() const
{
    boost::json::object payload;
    payload.emplace("roomId", room_id);
    payload.emplace("messages", serialize_messages(history.messages, usernames));
    payload.emplace("hasMoreMessages", history.has_more);
    return serialize_event("roomHistory", std::move(payload));
}
