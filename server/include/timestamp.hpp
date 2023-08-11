//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_TIMESTAMP_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_TIMESTAMP_HPP

#include <chrono>

namespace chat {

using timestamp_t = std::chrono::system_clock::time_point;

inline std::int64_t serialize_timestamp(timestamp_t input) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(input.time_since_epoch()).count();
}

inline timestamp_t parse_timestamp(std::int64_t input) noexcept
{
    return timestamp_t(std::chrono::milliseconds(input));
}

}  // namespace chat

#endif
