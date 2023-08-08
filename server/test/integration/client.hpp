//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_TEST_INTEGRATION_CLIENT_HPP
#define SERVERTECHCHAT_SERVER_TEST_INTEGRATION_CLIENT_HPP

#include <boost/core/span.hpp>

#include <memory>
#include <string_view>
#include <thread>

#include "application.hpp"

namespace chat {
namespace test {

class websocket_client
{
public:
    struct impl;

    websocket_client(impl*) noexcept;
    websocket_client(const websocket_client&) = delete;
    websocket_client(websocket_client&&) noexcept;
    websocket_client& operator=(const websocket_client&) = delete;
    websocket_client& operator=(websocket_client&&) noexcept;
    ~websocket_client();

    // Contents point into an internal buffer, valid until the next read
    std::string_view read();

    void write(std::string_view buffer);

private:
    std::unique_ptr<impl> impl_;
};

class server_runner_base
{
protected:
    application app;
    std::thread runner;

public:
    server_runner_base();
    server_runner_base(const server_runner_base&) = delete;
    server_runner_base(server_runner_base&&) = delete;
    server_runner_base& operator=(const server_runner_base&) = delete;
    server_runner_base& operator=(server_runner_base&&) = delete;
    ~server_runner_base();

    websocket_client connect_websocket();
};

class server_runner : public server_runner_base
{
public:
    server_runner();
};

}  // namespace test
}  // namespace chat

#endif
