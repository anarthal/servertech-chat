//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

module servertech_chat:email;

import std;

namespace chat {

// Returns true if the given string is a valid email (by pattern matching)
bool is_email(std::string_view str);

}  // namespace chat
