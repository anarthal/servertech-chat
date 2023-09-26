//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/email.hpp"

#include <boost/regex/v5/icu.hpp>
#include <boost/regex/v5/regex_match.hpp>

static constexpr std::string_view email_regex_str =
    R"REGEX(^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$)REGEX";

// Emails may contain Unicode characters. We use a Unicode-aware regular
// expression to validate them. This requires ICU support. If you don't need
// Unicode support, you can use plain boost::regex or std::regex, instead
static const auto email_regex = boost::make_u32regex(
    email_regex_str.begin(),
    email_regex_str.end(),
    boost::regex_constants::ECMAScript
);

bool chat::is_email(std::string_view email)
{
    return boost::regex_match(email.begin(), email.end(), email_regex);
}
