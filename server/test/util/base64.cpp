//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/base64.hpp"

#include <boost/core/span.hpp>
#include <boost/test/unit_test.hpp>

#include <string_view>

using namespace chat;
using namespace std::string_view_literals;

// Some test cases have been copied from CPython's and Android's base64 test suites

BOOST_AUTO_TEST_SUITE(base64)

constexpr struct
{
    std::string_view raw;
    std::string_view encoded;
    std::string_view encoded_no_padding;
} success_cases[] = {
    {"\0"sv, "AA==", "AA"},
    {"a"sv, "YQ==", "YQ"},
    {"ab"sv, "YWI=", "YWI"},
    {"abc"sv, "YWJj", "YWJj"},
    {""sv, "", ""},
    {
     "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789!@#0^&*();:<>,. []{}"sv, "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXpBQkNE"
        "RUZHSElKS0xNTk9QUVJTVFVWV1hZWjAxMjM0NT"
        "Y3ODkhQCMwXiYqKCk7Ojw+LC4gW117fQ==", "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXpBQkNE"
        "RUZHSElKS0xNTk9QUVJTVFVWV1hZWjAxMjM0NT"
        "Y3ODkhQCMwXiYqKCk7Ojw+LC4gW117fQ", },
    {"\xff"sv, "/w==", "/w"},
    {"\xff\xee"sv, "/+4=", "/+4"},
    {"\xff\xee\xdd"sv, "/+7d", "/+7d"},
    {"\xff\xee\xdd\xcc"sv, "/+7dzA==", "/+7dzA"},
    {"\xff\xee\xdd\xcc\xbb"sv, "/+7dzLs=", "/+7dzLs"},
    {"\xff\xee\xdd\xcc\xbb\xaa"sv, "/+7dzLuq", "/+7dzLuq"},
    {"\xff\xee\xdd\xcc\xbb\xaa\x99"sv, "/+7dzLuqmQ==", "/+7dzLuqmQ"},
    {"\xff\xee\xdd\xcc\xbb\xaa\x99\x88"sv, "/+7dzLuqmYg=", "/+7dzLuqmYg"},
};

BOOST_AUTO_TEST_CASE(encode)
{
    for (auto tc : success_cases)
    {
        BOOST_TEST_CONTEXT(tc.encoded)
        {
            auto actual = base64_encode({reinterpret_cast<const unsigned char*>(tc.raw.data()), tc.raw.size()}
            );
            BOOST_TEST(actual == tc.encoded);
        }
    }
}

BOOST_AUTO_TEST_CASE(encode_without_padding)
{
    for (auto tc : success_cases)
    {
        BOOST_TEST_CONTEXT(tc.encoded)
        {
            auto actual = base64_encode(
                {reinterpret_cast<const unsigned char*>(tc.raw.data()), tc.raw.size()},
                false
            );
            BOOST_TEST(actual == tc.encoded_no_padding);
        }
    }
}

BOOST_AUTO_TEST_CASE(decode_success)
{
    for (auto tc : success_cases)
    {
        BOOST_TEST_CONTEXT(tc.encoded)
        {
            auto actual = base64_decode(tc.encoded);
            BOOST_TEST_REQUIRE(actual.error() == error_code());
            auto actual_str = std::string_view(reinterpret_cast<const char*>(actual->data()), actual->size());
            BOOST_TEST(actual_str == tc.raw);
        }
    }
}

BOOST_AUTO_TEST_CASE(decode_success_without_padding)
{
    for (auto tc : success_cases)
    {
        BOOST_TEST_CONTEXT(tc.encoded_no_padding)
        {
            auto actual = base64_decode(tc.encoded_no_padding, false);
            BOOST_TEST_REQUIRE(actual.error() == error_code());
            auto actual_str = std::string_view(reinterpret_cast<const char*>(actual->data()), actual->size());
            BOOST_TEST(actual_str == tc.raw);
        }
    }
}

BOOST_AUTO_TEST_CASE(decode_error)
{
    constexpr std::string_view cases[] = {
        // Invalid characters
        "%3d=="sv,
        "$3d=="sv,
        "[=="sv,
        "YW]3="sv,
        "3{d=="sv,
        "3d}=="sv,
        "@@"sv,
        "!"sv,
        "YWJj\n"sv,
        "YWJj\nYWI="sv,

        // Bad padding
        "aGVsbG8sIHdvcmxk="sv,
        "aGVsbG8sIHdvcmxk=="sv,
        "aGVsbG8sIHdvcmxkPyE=="sv,
        "aGVsbG8sIHdvcmxkLg="sv,

        // Extra bytes
        "AA==A"sv,
        "AA==="sv,
    };

    for (auto tc : cases)
    {
        BOOST_TEST_CONTEXT(tc)
        {
            BOOST_TEST(base64_decode(tc).error() == error_code(errc::invalid_base64));
            BOOST_TEST(base64_decode(tc, false).error() == error_code(errc::invalid_base64));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
