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

#include <string>
#include <string_view>

#include "error.hpp"

namespace chat {

struct message
{
    std::string id;
    std::string username;
    std::string content;
};

using websocket_request = boost::variant2::variant<
    error_code,  // TODO: this should probably include some "reason" for 400-like responses
    message>;

// TODO: can we make this use string_view?
result<std::vector<message>> parse_room_history(const boost::redis::generic_response& from);
std::string serialize_redis_message(const message& msg);

std::string serialize_hello_event(
    const std::vector<std::string>& rooms,      // available rooms
    const std::vector<chat::message>& messages  // message history for 1st room
);
std::string serialize_messages_event(const std::vector<chat::message>& messages);

websocket_request parse_websocket_request(std::string_view from);

}  // namespace chat

#endif
