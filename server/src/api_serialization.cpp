//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "api_serialization.hpp"

#include <boost/describe/class.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_to.hpp>
#include <boost/variant2/variant.hpp>

#include <string_view>

#include "timestamp.hpp"

using namespace chat;

namespace chat {

// structs describing the wire format for business objects. Having these separate
// from business.hpp decouples from wire representation and business objects.
// It also allows to keep BOOST_DESCRIBE_STRUCT metadata in .cpp files, to
// improve build times.
//
// BOOST_DESCRIBE_STRUCT is used to add reflection capabilities to structs.
// It's used by boost::json::value_to, value_from and try_value_from to
// automatically generate JSON parsing/serializing code.

// User wire format
struct wire_user
{
    std::string id;
    std::string username;
};
BOOST_DESCRIBE_STRUCT(wire_user, (), (id, username))

// Message as received from the client; no timestamp or ID
struct wire_message
{
    wire_user user;
    std::string content;
};
BOOST_DESCRIBE_STRUCT(wire_message, (), (user, content))

// messages event wire format
struct wire_messages_event
{
    std::string roomId;
    std::vector<wire_message> messages;
};
BOOST_DESCRIBE_STRUCT(wire_messages_event, (), (roomId, messages))

// requestRoomHistory event wire format
struct wire_request_room_history
{
    std::string roomId;
    std::string firstMessageId;
};
BOOST_DESCRIBE_STRUCT(wire_request_room_history, (), (roomId, firstMessageId))

}  // namespace chat

// Helpers
static chat::message to_message(chat::wire_message&& from)
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
            {"id",        msg.id                            },
            {"content",   msg.content                       },
            {"user",
             {
                 {"id", msg.usr.id},
                 {"username", msg.usr.username},
             }                                              },
            {"timestamp", serialize_timestamp(msg.timestamp)},
        }));
    }
    return res;
}

static std::string serialize_event(std::string_view type, boost::json::object payload)
{
    boost::json::object evt;
    evt.emplace("type", type);
    evt.emplace("payload", std::move(payload));
    return boost::json::serialize(evt);
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
    boost::json::object payload;
    payload.emplace("rooms", std::move(json_rooms));
    return serialize_event("hello", std::move(payload));
}

std::string chat::serialize_messages_event(const messages_event& evt)
{
    boost::json::object payload;
    payload.emplace("roomId", evt.room_id);
    payload.emplace("messages", serialize_messages(evt.messages));
    return serialize_event("messages", std::move(payload));
}

std::string chat::serialize_room_history_event(const room_history_event& evt)
{
    boost::json::object payload;
    payload.emplace("roomId", evt.room_id);
    payload.emplace("messages", serialize_messages(evt.messages));
    payload.emplace("hasMoreMessages", evt.has_more_messages);
    return serialize_event("roomHistory", std::move(payload));
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

    // Parse the message, depending on its type
    if (type == "messages")
    {
        // Parse the payload
        auto parsed_payload = boost::json::try_value_to<wire_messages_event>(payload);
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
        auto parsed_payload = boost::json::try_value_to<wire_request_room_history>(payload);
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
