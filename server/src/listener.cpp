//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "listener.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>

#include <memory>

#include "error.hpp"
#include "http_session.hpp"
#include "shared_state.hpp"

using namespace chat;

static void do_listen(
    boost::asio::ip::tcp::acceptor acceptor,
    std::shared_ptr<chat::shared_state> st,
    boost::asio::yield_context yield
)
{
    error_code ec;

    while (true)
    {
        auto sock = acceptor.async_accept(yield[ec]);
        if (ec)
            return chat::log_error(ec, "accept");

        // Launch a new session for this connection
        boost::asio::spawn(
            sock.get_executor(),
            [state = st, socket = std::move(sock)](boost::asio::yield_context yield) mutable {
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
}

listener::listener(boost::asio::any_io_executor ex) : acceptor_(std::move(ex)) {}

error_code listener::setup(boost::asio::ip::tcp::endpoint listening_endpoint)
{
    error_code ec;

    // Open the acceptor
    acceptor_.open(listening_endpoint.protocol(), ec);
    if (ec)
        return ec;

    // Allow address reuse
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
        return ec;

    // Bind to the server address
    acceptor_.bind(listening_endpoint, ec);
    if (ec)
        return ec;

    // Start listening for connections
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
        return ec;

    return error_code();
}

void listener::run_until_completion(std::shared_ptr<shared_state> state)
{
    auto ex = acceptor_.get_executor();
    boost::asio::spawn(
        std::move(ex),
        [acceptor = std::move(acceptor_), st = std::move(state)](boost::asio::yield_context yield) mutable {
            do_listen(std::move(acceptor), std::move(st), yield);
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
