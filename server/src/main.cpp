//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>

#include "server.hpp"
#include "services/mysql_client.hpp"
#include "services/redis_client.hpp"
#include "shared_state.hpp"

namespace asio = boost::asio;
using namespace chat;

static void main_impl(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <address> <port> <doc_root>\n"
                  << "Example:\n"
                  << "    " << argv[0] << " 0.0.0.0 8080 .\n";
        exit(EXIT_FAILURE);
    }

    // Application config
    const char* doc_root = argv[3];                               // Path to static files
    const char* ip = argv[1];                                     // IP where the server will listen
    auto port = static_cast<unsigned short>(std::atoi(argv[2]));  // Port

    // An event loop, where the application will run. The server is single-
    // threaded, so we set the concurrency hint to 1
    asio::io_context ctx(1);

    // Singleton objects shared by all connections
    auto st = std::make_shared<shared_state>(doc_root, ctx.get_executor());

    // The physical endpoint where our server will listen
    asio::ip::tcp::endpoint listening_endpoint(asio::ip::make_address(ip), port);

    // A signal_set allows us to intercept SIGINT and SIGTERM and
    // exit gracefully
    asio::signal_set signals(ctx.get_executor(), SIGINT, SIGTERM);

    // Launch the Redis connection
    st->redis().start_run();

    // Launch the MySQL connection pool
    st->mysql().start_run();

    // Start listening for HTTP connections. This will run until the context is stopped
    asio::co_spawn(
        // The execution context to run the coroutine on
        ctx,

        // The actual coroutine to run, as an awaitable
        run_server(listening_endpoint, st),

        // Will run when the coroutine finishes. Propagate any exceptions thrown
        // in the coroutine to main
        [](std::exception_ptr exc) {
            if (exc)
                std::rethrow_exception(exc);
        }
    );

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    signals.async_wait([st, &ctx](boost::system::error_code, int) {
        // Stop the Redis reconnection loop
        st->redis().cancel();

        // Stop the MySQL reconnection loop
        st->mysql().cancel();

        // Stop the io_context. This will cause run() to return
        ctx.stop();
    });

    // Run the io_context. This will block until the context is stopped by
    // a signal and all outstanding async tasks are finished.
    ctx.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)
}

int main(int argc, char* argv[])
{
    try
    {
        main_impl(argc, argv);
    }
    catch (const std::exception& err)
    {
        std::cerr << "Exception in main(): " << err.what() << std::endl;
        return EXIT_FAILURE;
    }
}
