//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_BUSINESS_TYPES_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_BUSINESS_TYPES_HPP

#include <string>
#include <unordered_map>

#include "timestamp.hpp"

// This file contains business object definitions

namespace chat {

// An application user
struct user
{
    // User ID
    std::int64_t id;

    // Username
    std::string username;
};

// A user that includes details about authentication.
// Having this separate from the user definition avoids loading auth details
// unless strictly required.
struct auth_user
{
    // User ID
    std::int64_t id;

    // PHC-format password hash
    std::string hashed_password;
};

// A chat message
struct message
{
    // Message ID
    std::string id;

    // The actual content of the message
    std::string content;

    // UTC timestamp when the server received the message
    timestamp_t timestamp;

    // ID of the user that sent the message
    std::int64_t user_id{};
};

// A room history message batch
struct message_batch
{
    // The messages in the batch
    std::vector<message> messages;

    // true if there are more messages that could be loaded
    bool has_more{};
};

// A chat room
struct room
{
    // Room ID
    std::string id;

    // User-facing room name
    std::string name;

    // Initial room message history
    message_batch history;
};

// A map from user IDs to usernames
using username_map = std::unordered_map<std::int64_t, std::string>;

}  // namespace chat

#endif
