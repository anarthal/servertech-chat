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

// Sent to the client when it connects
struct hello_event
{
    std::vector<room> rooms;
};

// Sent by the client as a request to broadcast messages to other clients
// in a room. Also sent back to the client, when messages are broadcast.
struct messages_event
{
    std::string room_id;
    std::vector<message> messages;
};

// Sent by the client to request more history for a certain room.
struct request_room_history_event
{
    std::string room_id;

    // ID of the earliest-in-time message that the client has.
    // This is a pagination mechanism.
    std::string first_message_id;
};

// Sent to the client as a response to a request_room_history_event
struct room_history_event
{
    std::string room_id;
    std::vector<message> messages;
    bool has_more_messages;
};

// A variant that can represent any event that may be received from the client,
// or an error_code, if the client sent an invalid message
using any_client_event = boost::variant2::variant<
    error_code,  // Invalid, used to report errors
    messages_event,
    request_room_history_event>;

}  // namespace chat

#endif
