//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/signal_set.hpp>

#include <cstdlib>
#include <iostream>
#include <vector>

#include "error.hpp"
#include "listener.hpp"
#include "redis_client.hpp"
#include "shared_state.hpp"

using namespace chat;

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <address> <port> <doc_root>\n"
                  << "Example:\n"
                  << "    " << argv[0] << " 0.0.0.0 8080 .\n";
        return EXIT_FAILURE;
    }
    auto address = boost::asio::ip::make_address(argv[1]);
    auto port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto doc_root = argv[3];

    // The io_context is required for all I/O
    boost::asio::io_context ioc;

    // Shared state
    auto st = std::make_shared<shared_state>(doc_root, redis_client(ioc.get_executor()));

    // Launch the Redis connection
    st->redis().start_run();

    // Start listening for HTTP connections. This will run until the context is stopped
    run_listener(ioc, boost::asio::ip::tcp::endpoint{address, port}, st);

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc, st](error_code, int) {
        // Stop the io_context. This will cause run()
        // to return immediately, eventually destroying the
        // io_context and any remaining handlers in it.
        st->redis().cancel();
        ioc.stop();
    });

    // Run the I/O context
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)
    return EXIT_SUCCESS;
}
