//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "listener.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/async/wait_group.hpp>

#include <memory>

#include "error.hpp"
#include "http_session.hpp"
#include "shared_state.hpp"
#include "use_nothrow_op.hpp"

using namespace chat;

promise<error_code> chat::run_listener(
    boost::asio::ip::tcp::endpoint listening_endpoint,
    std::shared_ptr<shared_state> state
)
{
    boost::asio::ip::tcp::acceptor acceptor_(co_await boost::async::this_coro::executor);
    boost::async::wait_group gp;

    error_code ec;

    // Open the acceptor
    acceptor_.open(listening_endpoint.protocol(), ec);
    if (ec)
        co_return ec;

    // Allow address reuse
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
        co_return ec;

    // Bind to the server address
    acceptor_.bind(listening_endpoint, ec);
    if (ec)
        co_return ec;

    // Start listening for connections
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
        co_return ec;

    // Accept loop
    while (true)
    {
        // Accept a new connection
        auto [ec, sock] = co_await acceptor_.async_accept(use_nothrow_op);
        if (ec)
        {
            chat::log_error(ec, "accept");
            co_return ec;
        }

        // Launch a new session for this connection.
        // We should return listening for new connections, so this is launched in detached mode
        gp.reap();
        gp.push_back(run_http_session(std::move(sock), state));
    }
}
