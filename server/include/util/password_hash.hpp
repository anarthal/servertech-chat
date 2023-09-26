//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_PASSWORD_HASH_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_PASSWORD_HASH_HPP

#include <string>
#include <string_view>

namespace chat {

// Hashes a password using scrypt and a random salt. Returns a PHC-format string
// that can be inserted in DB and passed to verify_password
std::string hash_password(std::string_view passwd);

// Checks whether the incoming password matches the given hashed password
bool verify_password(std::string_view passwd, std::string_view hashed_passwd);

}  // namespace chat

#endif
