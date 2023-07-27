//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SRC_SERVER_SERIALIZATION_HPP
#define SERVERTECHCHAT_SRC_SERVER_SERIALIZATION_HPP

#include <boost/redis/resp3/node.hpp>
#include <boost/redis/response.hpp>
#include <boost/variant2/variant.hpp>

#include <chrono>
#include <string>
#include <string_view>

#include "error.hpp"

namespace chat {

using timestamp_t = std::chrono::system_clock::time_point;

// API objects
struct user
{
    std::string id;
    std::string username;
};

struct message
{
    std::string id;
    std::string content;
    user usr;
    timestamp_t timestamp;
};

struct room
{
    std::string id;
    std::string name;
    std::vector<message> messages;
    bool has_more_messages;
};

// Events received from the client
struct request_room_history_event
{
    std::string room_id;
    std::string first_message_id;
};

// Events to be sent to the client
struct hello_event
{
    std::vector<room> rooms;
};

struct messages_event
{
    std::string room_id;
    std::vector<message> messages;
};  // This may also be received from the client

struct room_history_event
{
    std::string room_id;
    std::vector<message> messages;
    bool has_more_messages;
};

// Represents any event that may be received from the client
using any_client_event = boost::variant2::variant<
    error_code,  // Invalid, used to report errors
    messages_event,
    request_room_history_event>;

// TODO: can we make this use string_view?
result<std::vector<std::vector<message>>> parse_room_history_batch(const boost::redis::generic_response& from
);
result<std::vector<std::string>> parse_string_list(const boost::redis::generic_response& from
);  // XADD pipeline responses
result<std::vector<message>> parse_room_history(const boost::redis::generic_response& from);
std::string serialize_redis_message(const message& msg);

std::string serialize_hello_event(const hello_event& evt);
std::string serialize_messages_event(const messages_event& evt);
std::string serialize_room_history_event(const room_history_event& evt);
any_client_event parse_client_event(std::string_view from);

}  // namespace chat

#endif
