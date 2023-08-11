//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_REDIS_SERIALIZATION_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_REDIS_SERIALIZATION_HPP

#include <boost/redis/response.hpp>

#include <vector>

#include "business.hpp"
#include "error.hpp"

// Contains function definitions to parse Redis responses. Used to
// implement redis_client. Boost.Redis doesn't support streams out
// of the box, so the parsing is non-trivial

namespace chat {

// TODO: can we make this use string_view?
result<std::vector<std::vector<message>>> parse_room_history_batch(const boost::redis::generic_response& from
);
result<std::vector<message>> parse_room_history(const boost::redis::generic_response& from);

// XADD pipeline responses. Calling execute with vector<string> doesn't work because
// Boost.Redis interprets it as a single response containing a list, instead of multiple
// responses containing a single string
result<std::vector<std::string>> parse_string_list(const boost::redis::generic_response& from);

std::string serialize_redis_message(const message& msg);

}  // namespace chat

#endif
