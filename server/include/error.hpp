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
#include <string_view>
#include <utility>

// Error management infrastructure. Uses Boost.System error codes and categories.
// This is consistent with Asio, Beast and Redis.

namespace chat {

// Type alias to reduce verbosity.
using error_code = boost::system::error_code;

// Type alias to reduce verbosity.
template <class T>
using result = boost::system::result<T>;

// An error code with a diagnostics message
struct error_with_message
{
    error_code ec;
    std::string msg;
};

// Required by the Boost.System infrastructure.
// Throws an exception from an error_with_message.
[[noreturn]] void throw_exception_from_error(const error_with_message& e, const boost::source_location&);

// Like result, but the error state includes an error message
template <class T>
using result_with_message = boost::system::result<T, error_with_message>;

// Error code enum for errors originated within our application
enum class errc
{
    redis_parse_error = 1,  // Data retrieved from Redis didn't match the format we expected
    redis_command_failed,   // A Redis command failed execution (e.g. we provided the wrong number of args)
    websocket_parse_error,  // Data received from the client didn't match the format we expected
    username_exists,        // couldn't create user, duplicate username
    email_exists,           // couldn't create user, duplicate username
    not_found,              // couldn't retrieve a certain resource, it doesn't exist
    invalid_password_hash,  // we found a password hash that was malformed
    already_exists,         // an entity can't be created because it already exists
    requires_auth,   // the requested resource requires authentication, but credentials haven't been provided
                     // or are invalid
    invalid_base64,  // attempt to decode an invalid base64 string
    uncaught_exception,    // an API handler threw an unexpected exception
    invalid_content_type,  // an endpoint received an unsupported Content-Type
};

// The error category for errc
const boost::system::error_category& get_chat_category() noexcept;

// Allows constructing error_code from errc
inline error_code make_error_code(errc v) noexcept
{
    return error_code(static_cast<int>(v), get_chat_category());
}

// Logs ec to stderr
void log_error(error_code ec, std::string_view what, std::string_view diagnostics = "");
inline void log_error(const error_with_message& err, std::string_view what)
{
    log_error(err.ec, what, err.msg);
}

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

// Returns an error_with_message with source-code location information on the error_code
#define CHAT_RETURN_ERROR_WITH_MESSAGE(e, msg)                                                   \
    {                                                                                            \
        static constexpr auto loc = BOOST_CURRENT_LOCATION;                                      \
        return ::chat::error_with_message{::chat::error_code(::chat::error_code(e), &loc), msg}; \
    }

#endif
