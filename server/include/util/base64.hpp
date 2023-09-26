//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_BASE64_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_BASE64_HPP

#include <boost/core/span.hpp>

#include <string>
#include <string_view>

#include "error.hpp"

namespace chat {

// Encodes the given input as a base64 string. If !with_padding, no padding
// will be added to the string.
std::string base64_encode(boost::span<const unsigned char> input, bool with_padding = true);

// Decodes the given input, interpreting it as a base64 string. If !with_padding,
// no padding is expected at the end of the string.
result<std::vector<unsigned char>> base64_decode(std::string_view input, bool with_padding = true);

}  // namespace chat

#endif
