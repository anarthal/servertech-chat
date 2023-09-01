//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_TIMESTAMP_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_TIMESTAMP_HPP

#include <chrono>

// Helpers to work with timestamps.
// The serialized representation of a timestamp is an int64_t with milliseconds
// since the UNIX epoch

namespace chat {

// Timestamps are eventually shown to the user, so we need them to match the system clock
using timestamp_t = std::chrono::system_clock::time_point;

// Converts a timestamp to its serialized representation
inline std::int64_t serialize_timestamp(timestamp_t input) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(input.time_since_epoch()).count();
}

// Creates a timestamp from its serialized representation
inline timestamp_t parse_timestamp(std::int64_t input) noexcept
{
    return timestamp_t(std::chrono::milliseconds(input));
}

}  // namespace chat

#endif
