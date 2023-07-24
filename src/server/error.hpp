//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SRC_SERVER_ERROR_HPP
#define SERVERTECHCHAT_SRC_SERVER_ERROR_HPP

#include <boost/assert/source_location.hpp>
#include <boost/system/error_category.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <string>
#include <utility>

namespace chat {

using error_code = boost::system::error_code;

template <class T>
using result = boost::system::result<T>;

enum class errc
{
    redis_parse_error = 1,
    websocket_parse_error
};

const boost::system::error_category& get_chat_category() noexcept;
inline error_code make_error_code(errc v) noexcept
{
    return error_code(static_cast<int>(v), get_chat_category());
}

void log_error(error_code ec, const char* what);

}  // namespace chat

namespace boost {
namespace system {

template <>
struct is_error_code_enum<chat::errc>
{
    static constexpr bool value = true;
};
}  // namespace system
}  // namespace boost

// There seems to be no constructor taking an error code enum & a source location
#define CHAT_RETURN_ERROR(e)                                    \
    {                                                           \
        static constexpr auto loc = BOOST_CURRENT_LOCATION;     \
        return ::chat::error_code(::chat::error_code(e), &loc); \
    }

#endif
