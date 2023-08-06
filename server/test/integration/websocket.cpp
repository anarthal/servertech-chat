//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "client.hpp"

using namespace chat::test;

BOOST_AUTO_TEST_CASE(a)
{
    server_runner runner;
    std::this_thread::sleep_for(std::chrono::seconds(2));  // TODO

    auto ws = runner.connect_websocket();
    auto buff = ws.read();

    // Close the WebSocket connection
    // ws.close(websocket::close_code::normal); TODO

    // The make_printable() function helps print a ConstBufferSequence
    std::cout << std::string_view((const char*)buff.data(), buff.size()) << std::endl;
}