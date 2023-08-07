//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <iostream>

#include "application.hpp"
#include "error.hpp"

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

    // Construct a config object
    chat::application_config config{
        argv[3],                                          // doc_root
        argv[1],                                          // ip
        static_cast<unsigned short>(std::atoi(argv[2])),  // port
    };

    // Construct an application object
    chat::application app(std::move(config));

    // Bind the app to the port and launch tasks
    auto ec = app.setup();
    if (ec)
    {
        log_error(ec, "Error setting up the application");
        exit(EXIT_FAILURE);
    }

    // Run the application until stopped by a signal
    app.run_until_completion(true);

    // (If we get here, it means we got a SIGINT or SIGTERM)
    return EXIT_SUCCESS;
}
