//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_BUSINESS_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_BUSINESS_HPP

#include <string>

#include "timestamp.hpp"

// This file contains business object definitions

namespace chat {

// An application user
struct user
{
    std::string id;
    std::string username;
};

// A message sent by a user
struct message
{
    std::string id;
    std::string content;
    user usr;  // Note: no auth is implemented yet
    timestamp_t timestamp;
};

// A chat-room, where messages are sent. Contains a limited set of the room's
// message history
struct room
{
    std::string id;
    std::string name;
    std::vector<message> messages;  // Last message is always first
    bool has_more_messages;         // Is there more message history we could load?
};

}  // namespace chat

#endif
