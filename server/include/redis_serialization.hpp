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

// Parses the result of getting the history for several rooms, in a batch
// (i.e. several batched XREVRANGEs)
result<std::vector<std::vector<message>>> parse_room_history_batch(const boost::redis::generic_response& from
);

// Parses the result of getting the history for a single room (i.e. a single XREVRANGE)
result<std::vector<message>> parse_room_history(const boost::redis::generic_response& from);

// Parses the response of a batch of XADDs. The response of each XADD is a string,
// containing the ID of the inserted record. Calling execute with vector<string>
// doesn't work because Boost.Redis will attempt to parse a single response containing
// an array of strings, instead of multiple responses with a single string
result<std::vector<std::string>> parse_batch_xadd_response(const boost::redis::generic_response& from);

// We store messages in streams as serialized JSON objects. Serialize
// a message into its JSON representation
std::string serialize_redis_message(const message& msg);

}  // namespace chat

#endif
