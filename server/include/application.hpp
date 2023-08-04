//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_APPLICATION_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_APPLICATION_HPP

#include <memory>
#include <string>

namespace chat {

struct application_config
{
    std::string doc_root;
    std::string ip;
    unsigned short port;
};

class application
{
    struct impl;
    std::unique_ptr<impl> impl_;

public:
    application(application_config config);
    application(const application&) = delete;
    application(application&&) noexcept;
    application& operator=(const application&) = delete;
    application& operator=(application&&) noexcept;
    ~application();

    // Launches the application and runs until explicitly stopped by stop().
    // If with_signal_handlers, registers signal handlers that call stop()
    void run_until_completion(bool with_signal_handlers);

    void stop();
};

}  // namespace chat

#endif
