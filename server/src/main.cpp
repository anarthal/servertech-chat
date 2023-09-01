//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

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

    // Application config
    const char* doc_root = argv[3];                               // Path to static files
    const char* ip = argv[1];                                     // IP where the server will listen
    auto port = static_cast<unsigned short>(std::atoi(argv[2]));  // Port

    // An event loop, where the application will run. The server is single-
    // threaded, so we set the concurrency hint to 1
    boost::asio::io_context ioc{1};

    // Singleton objects shared by all connections
    auto st = std::make_shared<shared_state>(doc_root, redis_client(ioc.get_executor()));

    // The physical endpoint where our server will listen
    boost::asio::ip::tcp::endpoint listening_endpoint{boost::asio::ip::make_address(ip), port};

    // A signal_set allows us to intercept SIGINT and SIGTERM and
    // exit gracefully
    boost::asio::signal_set signals{ioc.get_executor(), SIGINT, SIGTERM};

    // Launch the Redis connection
    st->redis().start_run();

    // Start listening for HTTP connections. This will run until the context is stopped
    auto ec = launch_http_listener(ioc.get_executor(), listening_endpoint, st);
    if (ec)
    {
        log_error(ec, "Error launching the HTTP listener");
        exit(EXIT_FAILURE);
    }

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    signals.async_wait([st, &ioc](error_code, int) {
        // Stop the Redis reconnection loop
        st->redis().cancel();

        // Stop the io_context. This will cause run() to return
        ioc.stop();
    });

    // Run the io_context. This will block until the context is stopped by
    // a signal and all outstanding async tasks are finished.
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)
    return EXIT_SUCCESS;
}
