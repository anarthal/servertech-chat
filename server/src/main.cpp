//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/async/main.hpp>

#include <cstdlib>
#include <iostream>

#include "application.hpp"
#include "error.hpp"

using namespace chat;
namespace async = boost::async;

async::main co_main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <address> <port> <doc_root>\n"
                  << "Example:\n"
                  << "    " << argv[0] << " 0.0.0.0 8080 .\n";
        co_return EXIT_FAILURE;
    }

    // Construct a config object
    chat::application_config config{
        argv[3],                                          // doc_root
        argv[1],                                          // ip
        static_cast<unsigned short>(std::atoi(argv[2])),  // port
    };

    // Run the application until completion
    auto ec = co_await chat::run_application(config);
    if (ec)
    {
        log_error(ec, "Error setting up the application");
        co_return EXIT_FAILURE;
    }

    // (If we get here, it means we got a SIGINT or SIGTERM)
    co_return EXIT_SUCCESS;
}
