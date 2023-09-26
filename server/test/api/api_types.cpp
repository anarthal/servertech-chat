//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "api/api_types.hpp"

#include <boost/json/error.hpp>
#include <boost/json/parse.hpp>
#include <boost/test/tools/interface.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/variant2/variant.hpp>

#include <string_view>

#include "business_types.hpp"
#include "error.hpp"
#include "timestamp.hpp"

using namespace chat;

BOOST_AUTO_TEST_SUITE(api_types)

//
// Incoming types
//

// create_account_request
BOOST_AUTO_TEST_CASE(create_account_request_from_json)
{
    // Data
    const char* from = R"%({
        "username": "somenick",
        "email": "test@test.com",
        "password": "Useruser10!"
    })%";

    // Call the function
    auto result = create_account_request::from_json(from);

    // Validate
    BOOST_TEST_REQUIRE(result.error() == error_code());
    BOOST_TEST(result->username == "somenick");
    BOOST_TEST(result->email == "test@test.com");
    BOOST_TEST(result->password == "Useruser10!");
}

// login_request
BOOST_AUTO_TEST_CASE(login_request_from_json)
{
    // Data
    const char* from = R"%({
        "email": "test@test.com",
        "password": "Useruser10!"
    })%";

    // Call the function
    auto result = login_request::from_json(from);

    // Validate
    BOOST_TEST_REQUIRE(result.error() == error_code());
    BOOST_TEST(result->email == "test@test.com");
    BOOST_TEST(result->password == "Useruser10!");
}

// parse_client_event
BOOST_AUTO_TEST_CASE(parse_client_event_messages_success)
{
    // Data
    const char* input = R"%({
        "type": "clientMessages",
        "payload": {
            "roomId": "myRoom",
            "messages": [{
                "content":"hello room 1!"
            }]
        }
    })%";

    // Call the function
    auto evt_variant = parse_client_event(input);

    // Validate
    const auto& evt = boost::variant2::get<client_messages_event>(evt_variant);
    BOOST_TEST(evt.roomId == "myRoom");
    BOOST_TEST(evt.messages.size() == 1u);
    BOOST_TEST(evt.messages[0].content == "hello room 1!");
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
    BOOST_TEST(evt.roomId == "myRoom");
    BOOST_TEST(evt.firstMessageId == "100-0");
}

BOOST_AUTO_TEST_CASE(parse_client_event_error_missing_key)
{
    // Data
    const char* input = R"%({
        "type": "clientMessages",
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

//
// Outgoing types
//

// api_error
BOOST_AUTO_TEST_CASE(api_error_to_json)
{
    // Data
    api_error err{api_error_id::email_exists, "Something happened"};

    // Call the function
    auto serialized = err.to_json();

    // Validate
    const char* expected = R"%({
        "id": "EMAIL_EXISTS",
        "message": "Something happened"
    })%";
    BOOST_TEST(boost::json::parse(serialized) == boost::json::parse(expected));
}

// hello_event
BOOST_AUTO_TEST_CASE(hello_event_to_json)
{
    // Data
    message_batch room1_history{
        {
         {"100-0", "hello room 1!", parse_timestamp(123), 11},
         {"101-0", "hello back!", parse_timestamp(125), 12},
         },
        true,
    };
    std::vector<room> rooms{
        {"room1", "Room name 1", std::move(room1_history)},
        {"room2", "Room name 2"}
    };
    username_map usernames{
        {11, "username1"},
        {12, "username2"},
        {13, "other"    }
    };
    user me{12, "username2"};
    hello_event evt{me, rooms, usernames};

    // Call the function
    auto serialized = evt.to_json();

    // Validate
    const char* expected = R"%({
        "type": "hello",
        "payload": {
            "me": {
                "id": 12,
                "username": "username2"
            },
            "rooms":[{
                "id": "room1",
                "name": "Room name 1",
                "messages":[{
                    "id": "100-0",
                    "content":"hello room 1!",
                    "user": {"id": 11, "username": "username1" },
                    "timestamp": 123
                }, {
                    "id": "101-0",
                    "content": "hello back!",
                    "user": {"id": 12, "username": "username2" },
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

// sever_messages_event
BOOST_AUTO_TEST_CASE(server_messages_event_to_json)
{
    // Data
    std::vector<message> msgs{
        {"100-0", "hello room 1!", parse_timestamp(123), 11},
        {"101-0", "hello back!",   parse_timestamp(125), 11},
    };
    user sending_user{11, "username1"};
    server_messages_event evt{"myRoom", sending_user, msgs};

    // Call the function
    auto serialized = evt.to_json();

    // Validate
    const char* expected = R"%({
        "type": "serverMessages",
        "payload": {
            "roomId": "myRoom",
            "messages": [{
                "id": "100-0",
                "content":"hello room 1!",
                "user": {"id": 11, "username": "username1" },
                "timestamp": 123
            }, {
                "id": "101-0",
                "content": "hello back!",
                "user": {"id": 11, "username": "username1" },
                "timestamp": 125
            }]
        }
    })%";
    BOOST_TEST(boost::json::parse(serialized) == boost::json::parse(expected));
}

// room_history_event
BOOST_AUTO_TEST_CASE(room_history_event_to_json)
{
    // Data
    message_batch msg_batch{
        {
         {"100-0", "hello room 1!", parse_timestamp(123), 11},
         {"101-0", "hello back!", parse_timestamp(125), 12},
         },
        true,
    };
    username_map usernames{
        {11, "username1"},
        {12, "username2"},
        {13, "other"    }
    };
    room_history_event evt{"myRoom", msg_batch, usernames};

    // Call the function
    auto serialized = evt.to_json();

    // Validate
    const char* expected = R"%({
        "type": "roomHistory",
        "payload": {
            "roomId": "myRoom",
            "hasMoreMessages": true,
            "messages": [{
                "id": "100-0",
                "content":"hello room 1!",
                "user": {"id": 11, "username": "username1" },
                "timestamp": 123
            }, {
                "id": "101-0",
                "content": "hello back!",
                "user": {"id": 12, "username": "username2" },
                "timestamp": 125
            }]
        }
    })%";
    BOOST_TEST(boost::json::parse(serialized) == boost::json::parse(expected));
}

BOOST_AUTO_TEST_SUITE_END()