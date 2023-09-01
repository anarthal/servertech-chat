//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_ERROR_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_ERROR_HPP

#include <boost/assert/source_location.hpp>
#include <boost/system/error_category.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <string>
#include <utility>

// Error management infrastructure. Uses Boost.System error codes and categories.
// This is consistent with Asio, Beast and Redis.

namespace chat {

// Type alias to reduce verbosity.
using error_code = boost::system::error_code;

// Type alias to reduce verbosity.
template <class T>
using result = boost::system::result<T>;

// Error code enum for errors originated within our application
enum class errc
{
    redis_parse_error = 1,  // Data retrieved from Redis didn't match the format we expected
    websocket_parse_error   // Data received from the client didn't match the format we expected
};

// The error category for errc
const boost::system::error_category& get_chat_category() noexcept;

// Allows constructing error_code from errc
inline error_code make_error_code(errc v) noexcept
{
    return error_code(static_cast<int>(v), get_chat_category());
}

// Logs ec to stderr
void log_error(error_code ec, const char* what);

}  // namespace chat

// Allows constructing error_code from errc
namespace boost {
namespace system {

template <>
struct is_error_code_enum<chat::errc>
{
    static constexpr bool value = true;
};
}  // namespace system
}  // namespace boost

// Returns an error_code with source-code location information on it
#define CHAT_RETURN_ERROR(e)                                    \
    {                                                           \
        static constexpr auto loc = BOOST_CURRENT_LOCATION;     \
        return ::chat::error_code(::chat::error_code(e), &loc); \
    }

#endif
