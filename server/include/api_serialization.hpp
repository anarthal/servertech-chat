//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_API_SERIALIZATION_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_API_SERIALIZATION_HPP

#include "api.hpp"

// This file contains functions to (de)serialize API objects

namespace chat {

std::string serialize_hello_event(const hello_event& evt);
std::string serialize_messages_event(const messages_event& evt);
std::string serialize_room_history_event(const room_history_event& evt);
any_client_event parse_client_event(std::string_view from);

}  // namespace chat

#endif