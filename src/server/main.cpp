//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

//------------------------------------------------------------------------------
/*
    WebSocket chat server, multi-threaded

    This implements a multi-user chat room using WebSocket. The
    `io_context` runs on any number of threads, specified at
    the command line.

*/
//------------------------------------------------------------------------------

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/redis/connection.hpp>
#include <boost/smart_ptr/make_shared_array.hpp>

#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "error.hpp"
#include "listener.hpp"
#include "shared_state.hpp"

using namespace chat;

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 5)
    {
        std::cerr << "Usage: websocket-chat-multi <address> <port> <doc_root> <threads>\n"
                  << "Example:\n"
                  << "    websocket-chat-server 0.0.0.0 8080 . 5\n";
        return EXIT_FAILURE;
    }
    auto address = boost::asio::ip::make_address(argv[1]);
    auto port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto doc_root = argv[3];
    auto const threads = std::max<int>(1, std::atoi(argv[4]));

    // The io_context is required for all I/O
    boost::asio::io_context ioc;

    // Redis connection
    boost::redis::connection conn(ioc.get_executor());

    // Create and launch a listening port
    listener list(
        ioc,
        boost::asio::ip::tcp::endpoint{address, port},
        std::make_shared<shared_state>(doc_root)
    );
    list.start();

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc](error_code, int) {
        // Stop the io_context. This will cause run()
        // to return immediately, eventually destroying the
        // io_context and any remaining handlers in it.
        ioc.stop();
    });

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)

    // Block until all the threads exit
    for (auto& t : v)
        t.join();

    return EXIT_SUCCESS;
}
