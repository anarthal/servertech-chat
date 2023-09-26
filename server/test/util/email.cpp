//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/email.hpp"

#include <boost/test/unit_test.hpp>

#include <string_view>

// Source: https://gist.github.com/cjaoude/fd9910626629b53c4d25

using namespace chat;

BOOST_AUTO_TEST_SUITE(email)

BOOST_AUTO_TEST_CASE(is_email_valid)
{
    constexpr std::string_view test_cases[] = {
        "email@example.com",
        "firstname.lastname@example.com",
        "email@subdomain.example.com",
        "firstname+lastname@example.com",
        "\"email\"@example.com",
        "1234567890@example.com",
        "email@example-one.com",
        "_______@example.com",
        "email@example.name",
        "email@example.museum",
        "email@example.co.jp",
        "firstname-lastname@example.com",
        "\xc3\xb1@example.com",  // spanish enye, UTF-8 encoded
    };

    for (auto tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc) { BOOST_TEST(is_email(tc)); }
    }
}

BOOST_AUTO_TEST_CASE(is_email_invalid)
{
    constexpr std::string_view test_cases[] = {
        "plainaddress",
        "#@%^%#$@#$@#.com",
        "@example.com",
        "email.example.com",
        "email@example@example.com",
        ".email@example.com",
        "email.@example.com",
        "email..email@example.com",
        "email@example.com (Joe Smith)",
        "email@example",
        "email@example..com",
        "Abc..123@example.com",
        "”(),:;<>[\\]@example.com",
        "this\\ is\"really\"not\\allowed@example.com",
    };

    for (auto tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc) { BOOST_TEST(!is_email(tc)); }
    }
}

BOOST_AUTO_TEST_SUITE_END()
