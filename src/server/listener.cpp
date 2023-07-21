//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "listener.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/bind_handler.hpp>

#include <functional>
#include <iostream>

#include "http_session.hpp"
#include "shared_state.hpp"

static void report(chat::error_code ec, char const* what)
{
    // Don't report on canceled operations
    if (ec == boost::asio::error::operation_aborted)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}

void chat::listener::fail(chat::error_code ec, char const* what)
{
    report(ec, what);
    ioc_.stop();
}

chat::listener::listener(
    boost::asio::io_context& ioc,
    boost::asio::ip::tcp::endpoint endpoint,
    std::shared_ptr<shared_state> st
)
    : ioc_(ioc), acceptor_(ioc), state_(std::move(st))
{
    error_code ec;

    // Open the acceptor
    acceptor_.open(endpoint.protocol(), ec);
    if (ec)
    {
        fail(ec, "open");
        return;
    }

    // Allow address reuse
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
    {
        fail(ec, "set_option");
        return;
    }

    // Bind to the server address
    acceptor_.bind(endpoint, ec);
    if (ec)
    {
        fail(ec, "bind");
        return;
    }

    // Start listening for connections
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        fail(ec, "listen");
        return;
    }
}

void chat::listener::start() { launch_listener(); }

// Handle a connection
void chat::listener::on_accept(error_code ec, boost::asio::ip::tcp::socket socket)
{
    if (ec)
        return report(ec, "accept");
    else
    {
        // Launch a new session for this connection
        boost::asio::any_io_executor ex(socket.get_executor());
        boost::asio::spawn(
            std::move(ex),
            [state = this->state_, socket = std::move(socket)](boost::asio::yield_context yield) mutable {
                run_http_session(std::move(socket), std::move(state), yield);
            },
            // we ignore the result of the session,
            // most errors are handled with error_code
            [](std::exception_ptr ex) {
                // if an exception occurred in the coroutine,
                // it's something critical, e.g. out of memory
                // we capture normal errors in the ec
                // so we just rethrow the exception here,
                // which will cause `ioc.run()` to throw
                if (ex)
                    std::rethrow_exception(ex);
            }
        );
    }

    launch_listener();
}

void chat::listener::launch_listener()
{
    acceptor_.async_accept(std::bind(&listener::on_accept, this, std::placeholders::_1, std::placeholders::_2)
    );
}
