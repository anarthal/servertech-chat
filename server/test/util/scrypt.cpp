//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/scrypt.hpp"

#include <boost/test/tools/context.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <string_view>

#include "error.hpp"

using namespace chat;
using namespace std::string_view_literals;

BOOST_AUTO_TEST_SUITE(scrypt)

constexpr std::array<unsigned char, salt_size> salt{0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                                    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                                                    22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
constexpr std::array<unsigned char, hash_size> hash{105, 111, 219, 25,  213, 94,  89,  177, 154, 146, 220,
                                                    253, 78,  58,  157, 192, 65,  141, 59,  80,  51,  123,
                                                    12,  124, 19,  174, 85,  245, 216, 242, 192, 197};

BOOST_AUTO_TEST_CASE(phc_parse_success)
{
    struct
    {
        std::string_view name;
        std::string_view input;
        scrypt_data expected;
    } test_cases[] = {
        {
         "regular", "$scrypt$ln=16,r=8,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/"
            "bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU", {
                {16, 8, 1},
                {salt.begin(), salt.end()},
                {hash.begin(), hash.end()},
            }, },
        {"default params",
         "$scrypt$unknown=10$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/"
         "bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU", {
             {14, 8, 1},
             {salt.begin(), salt.end()},
             {hash.begin(), hash.end()},
         }},
        {
         "different lengths", "$scrypt$ln=16,r=8,p=1$AAECAwQFBgcICQoLDA0ODw$aW/bGdVeWbGaktz9TjqdwA",
         {
                {16, 8, 1},
                {salt.begin(), salt.begin() + 16},
                {hash.begin(), hash.begin() + 16},
            }, },
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            auto result = scrypt_phc_parse(tc.input);
            BOOST_TEST_REQUIRE(result.error() == error_code());
            BOOST_TEST(result->params.ln == tc.expected.params.ln);
            BOOST_TEST(result->params.r == tc.expected.params.r);
            BOOST_TEST(result->params.p == tc.expected.params.p);
            BOOST_TEST(result->salt == tc.expected.salt);
            BOOST_TEST(result->hash == tc.expected.hash);
        }
    }
}

BOOST_AUTO_TEST_CASE(phc_parse_error)
{
    // clang-format off
    constexpr std::string_view test_cases[] = {
        "$other$ln=13,r=4,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$$ln=13,r=4,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln=13,r=4,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8",
        "scrypt$ln=13,r=4,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln13,r=4$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln=13,r$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln=13,r=4,p=1$bad_base64$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln=13,r=4,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$bad_base64",
        "$scrypt$ln=13,r=4,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU$extra",
        "$scrypt$ln=13,r=4,p=999$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln=999,r=4,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln=0,r=4,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln=13,r=0,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln=13,r=4,p=0$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln=0.23,r=4,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln=13,r=0.23,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "$scrypt$ln=13,r=4,p=0.23$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU",
        "",
    };
    // clang-format on

    for (auto tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc)
        {
            auto result = scrypt_phc_parse(tc);
            BOOST_TEST(result.error() == error_code(errc::invalid_password_hash));
        }
    }
}

BOOST_AUTO_TEST_CASE(phc_serialize)
{
    scrypt_params params{16, 8, 1};
    auto value = scrypt_phc_serialize(params, salt, hash);
    BOOST_TEST(
        value ==
        "$scrypt$ln=16,r=8,p=1$AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8$aW/"
        "bGdVeWbGaktz9TjqdwEGNO1Azewx8E65V9djywMU"
    );
}

BOOST_AUTO_TEST_CASE(generate_hash)
{
    // Hashing with n=16 consumes too much memory
    constexpr std::array<unsigned char, hash_size> expected{
        127, 67,  110, 163, 145, 163, 201, 126, 39,  101, 224, 211, 113, 160, 89, 242,
        192, 191, 37,  112, 19,  70,  167, 73,  168, 158, 74,  71,  219, 195, 5,  85,
    };

    // Encode a password. Non-ASCII characters supported.
    auto value = scrypt_generate_hash("p!ass\0word\xc3\xb1"sv, scrypt_params{13, 8, 1}, salt);

    // Verify
    BOOST_TEST(value == expected);
}

BOOST_AUTO_TEST_CASE(time_safe_equals_)
{
    struct
    {
        std::string_view name;
        std::vector<unsigned char> lhs;
        std::vector<unsigned char> rhs;
        bool expected;
    } test_cases[] = {
        {"empty_empty",    {},           {},     true },
        {"nonempty_empty", {1, 2},       {},     false},
        {"equals",         {1, 2},       {1, 2}, true },
        {"prefix",         {1, 2, 3, 4}, {1, 2}, false},
        {"different",      {5, 6},       {1, 2}, false},
    };

    for (const auto& tc : test_cases)
    {
        BOOST_TEST_CONTEXT(tc.name)
        {
            BOOST_TEST(time_safe_equals(tc.lhs, tc.rhs) == tc.expected);
            BOOST_TEST(time_safe_equals(tc.rhs, tc.lhs) == tc.expected);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
