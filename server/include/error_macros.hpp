//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_ERROR_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_ERROR_HPP

// Returns an error_code with source-code location information on it
#define CHAT_RETURN_ERROR(e)                                                      \
    {                                                                             \
        static constexpr auto loc = BOOST_CURRENT_LOCATION;                       \
        return ::boost::system::error_code(::boost::system::error_code(e), &loc); \
    }

// Same, but for co_return
#define CHAT_CO_RETURN_ERROR(e)                                                      \
    {                                                                                \
        static constexpr auto loc = BOOST_CURRENT_LOCATION;                          \
        co_return ::boost::system::error_code(::boost::system::error_code(e), &loc); \
    }

#endif
