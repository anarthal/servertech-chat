//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/password_hash.hpp"

#include <boost/test/unit_test.hpp>

#include <string_view>

using namespace chat;

BOOST_AUTO_TEST_SUITE(password_hash)

BOOST_AUTO_TEST_CASE(success)
{
    constexpr std::string_view pasword = "some_password";

    // Hash the password
    auto hash = hash_password(pasword);

    // It was hashed using scrypt and encoded using PHC
    std::string_view prefix = "$scrypt$";
    BOOST_TEST(hash.substr(0, prefix.size()) == prefix);

    // Hashing the password again yields a different value because of the salt
    auto hash2 = hash_password(pasword);
    BOOST_TEST(hash != hash2);

    // Checking the right password succeeds
    BOOST_TEST(verify_password(pasword, hash));

    // Checking an incorrect password fails
    BOOST_TEST(!verify_password("bad_password", hash));
}

BOOST_AUTO_TEST_SUITE_END()
