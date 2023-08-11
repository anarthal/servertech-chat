//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "api_serialization.hpp"

#include <boost/json/error.hpp>
#include <boost/json/parse.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/variant2/variant.hpp>

#include "api.hpp"
#include "business.hpp"
#include "error.hpp"
#include "timestamp.hpp"

using namespace chat;

BOOST_AUTO_TEST_SUITE(api_serialization)

// serialize_hello_event
BOOST_AUTO_TEST_CASE(serialize_hello_event_success)
{
    // Data
    hello_event evt;
    evt.rooms.push_back(room{
        "room1",
        "Room name 1",
        {
          {"100-0", "hello room 1!", {"user1", "username1"}, parse_timestamp(123)},
          {"101-0", "hello back!", {"user2", "username2"}, parse_timestamp(125)},
          },
        true
    });
    evt.rooms.push_back(room{"room2", "Room name 2", {}, false});

    // Call the function
    auto serialized = serialize_hello_event(evt);

    // Validate
    const char* expected = R"%({
        "type": "hello",
        "payload": {
            "rooms":[{
                "id": "room1",
                "name": "Room name 1",
                "messages":[{
                    "id": "100-0",
                    "content":"hello room 1!",
                    "user": {"id": "user1", "username": "username1" },
                    "timestamp": 123
                }, {
                    "id": "101-0",
                    "content": "hello back!",
                    "user": {"id": "user2", "username": "username2" },
                    "timestamp": 125
                }],
                "hasMoreMessages": true
            }, {
                "id": "room2",
                "name": "Room name 2",
                "messages": [],
                "hasMoreMessages": false
            }]
        }
    })%";
    BOOST_TEST(boost::json::parse(serialized) == boost::json::parse(expected));
}

// serialize_messages_event
BOOST_AUTO_TEST_CASE(serialize_messages_event_success)
{
    // Data
    messages_event evt{
        "myRoom",
        {
          {"100-0", "hello room 1!", {"user1", "username1"}, parse_timestamp(123)},
          {"101-0", "hello back!", {"user2", "username2"}, parse_timestamp(125)},
          }
    };

    // Call the function
    auto serialized = serialize_messages_event(evt);

    // Validate
    const char* expected = R"%({
        "type": "messages",
        "payload": {
            "roomId": "myRoom",
            "messages": [{
                "id": "100-0",
                "content":"hello room 1!",
                "user": {"id": "user1", "username": "username1" },
                "timestamp": 123
            }, {
                "id": "101-0",
                "content": "hello back!",
                "user": {"id": "user2", "username": "username2" },
                "timestamp": 125
            }]
        }
    })%";
    BOOST_TEST(boost::json::parse(serialized) == boost::json::parse(expected));
}

// serialize_room_history_event
BOOST_AUTO_TEST_CASE(serialize_room_history_event_success)
{
    // Data
    room_history_event evt{
        "myRoom",
        {
          {"100-0", "hello room 1!", {"user1", "username1"}, parse_timestamp(123)},
          {"101-0", "hello back!", {"user2", "username2"}, parse_timestamp(125)},
          },
        true
    };

    // Call the function
    auto serialized = serialize_room_history_event(evt);

    // Validate
    const char* expected = R"%({
        "type": "roomHistory",
        "payload": {
            "roomId": "myRoom",
            "hasMoreMessages": true,
            "messages": [{
                "id": "100-0",
                "content":"hello room 1!",
                "user": {"id": "user1", "username": "username1" },
                "timestamp": 123
            }, {
                "id": "101-0",
                "content": "hello back!",
                "user": {"id": "user2", "username": "username2" },
                "timestamp": 125
            }]
        }
    })%";
    BOOST_TEST(boost::json::parse(serialized) == boost::json::parse(expected));
}

// parse_client_event
BOOST_AUTO_TEST_CASE(parse_client_event_messages_success)
{
    // Data
    const char* input = R"%({
        "type": "messages",
        "payload": {
            "roomId": "myRoom",
            "messages": [{
                "content":"hello room 1!",
                "user": {"id": "user1", "username": "username1" }
            }]
        }
    })%";

    // Call the function
    auto evt_variant = parse_client_event(input);

    // Validate
    const auto& evt = boost::variant2::get<messages_event>(evt_variant);
    BOOST_TEST(evt.room_id == "myRoom");
    BOOST_TEST(evt.messages.size() == 1u);
    BOOST_TEST(evt.messages[0].content == "hello room 1!");
    BOOST_TEST(evt.messages[0].usr.id == "user1");
    BOOST_TEST(evt.messages[0].usr.username == "username1");
}

BOOST_AUTO_TEST_CASE(parse_client_event_request_room_history_success)
{
    // Data
    const char* input = R"%({
        "type": "requestRoomHistory",
        "payload": {
            "roomId": "myRoom",
            "firstMessageId": "100-0"
        }
    })%";

    // Call the function
    auto evt_variant = parse_client_event(input);

    // Validate
    const auto& evt = boost::variant2::get<request_room_history_event>(evt_variant);
    BOOST_TEST(evt.room_id == "myRoom");
    BOOST_TEST(evt.first_message_id == "100-0");
}

BOOST_AUTO_TEST_CASE(parse_client_event_error_missing_key)
{
    // Data
    const char* input = R"%({
        "type": "messages",
        "payload": {
            "roomId": "myRoom"
        }
    })%";

    // Call the function
    auto evt_variant = parse_client_event(input);

    // Validate
    auto ec = boost::variant2::get<error_code>(evt_variant);
    BOOST_TEST(ec == error_code(boost::json::error::unknown_name));
}

BOOST_AUTO_TEST_CASE(parse_client_event_error_unknown_type)
{
    // Data
    const char* input = R"%({
        "type": "bad",
        "payload": {
            "roomId": "myRoom"
        }
    })%";

    // Call the function
    auto evt_variant = parse_client_event(input);

    // Validate
    auto ec = boost::variant2::get<error_code>(evt_variant);
    BOOST_TEST(ec == error_code(errc::websocket_parse_error));
}

BOOST_AUTO_TEST_SUITE_END()