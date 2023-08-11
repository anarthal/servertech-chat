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

}  // namespace chat

#endif
