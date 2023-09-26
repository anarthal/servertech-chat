//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_EMAIL_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_EMAIL_HPP

namespace chat {

// Returns true if the given string is a valid email (by pattern matching)
bool is_email(std::string_view str);

}  // namespace chat

#endif
