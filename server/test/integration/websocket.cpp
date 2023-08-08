//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/json/parse.hpp>
#include <boost/test/unit_test.hpp>

#include <iostream>

#include "client.hpp"

using namespace chat::test;

BOOST_AUTO_TEST_CASE(no_messages)
{
    server_runner runner;
    auto ws = runner.connect_websocket();

    // Receive a hello event
    auto buff = ws.read();
    auto jv = boost::json::parse(buff);

    // clang-format off
    boost::json::value expected = {
        {"type", "hello"},
        {"payload", {
            {"rooms", {
                {
                    {"id", "beast"},
                    {"name", "Boost.Beast"},
                    {"messages", boost::json::array()},
                    {"hasMoreMessages", false},
                },
                {
                    {"id", "async"},
                    {"name", "Boost.Async"},
                    {"messages", boost::json::array()},
                    {"hasMoreMessages", false},
                },
                {
                    {"id", "db"},
                    {"name", "Database connectors"},
                    {"messages", boost::json::array()},
                    {"hasMoreMessages", false},
                },
                {
                    {"id", "wasm"},
                    {"name", "Web assembly"},
                    {"messages", boost::json::array()},
                    {"hasMoreMessages", false},
                },
            }}
        }}
    };
    // clang-format on

    // Verify it
    BOOST_TEST(jv == expected);
}