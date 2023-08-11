//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_API_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_API_HPP

#include <boost/variant2/variant.hpp>

#include "business.hpp"
#include "error.hpp"

// This file contains type definitions for HTTP and websocket API objects

namespace chat {

// Events sent to the client
struct hello_event
{
    std::vector<room> rooms;
};

struct messages_event
{
    std::string room_id;
    std::vector<message> messages;
};

struct room_history_event
{
    std::string room_id;
    std::vector<message> messages;
    bool has_more_messages;
};

// Events received from the client
struct request_room_history_event
{
    std::string room_id;
    std::string first_message_id;
};

// Represents any event that may be received from the client
using any_client_event = boost::variant2::variant<
    error_code,  // Invalid, used to report errors
    messages_event,
    request_room_history_event>;

}  // namespace chat

#endif
