//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "server.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/this_coro.hpp>

#include <exception>
#include <memory>

#include "error.hpp"
#include "http_session.hpp"
#include "shared_state.hpp"

namespace asio = boost::asio;
using namespace chat;

static void log_exception(std::exception_ptr ptr)
{
    try
    {
        // Rethrowing is the only way to access the underlying exception object
        std::rethrow_exception(ptr);
    }
    catch (const std::exception& exc)
    {
        log_error(errc::uncaught_exception, "Uncaught exception in HTTP session handler", exc.what());
    }
}

asio::awaitable<void> chat::run_server(
    asio::ip::tcp::endpoint listening_endpoint,
    std::shared_ptr<shared_state> st
)
{
    // Get the executor associated to the current coroutine
    auto ex = co_await asio::this_coro::executor;

    // An object that allows us to accept incoming TCP connections
    asio::ip::tcp::acceptor acceptor(ex);

    // Set up the acceptor to listen in the requested endpoint and allow
    // address reuse.
    acceptor.open(listening_endpoint.protocol());
    acceptor.set_option(asio::socket_base::reuse_address(true));
    acceptor.bind(listening_endpoint);
    acceptor.listen();

    // We accept connections in an infinite loop. When the io_context is stopped,
    // the coroutine will no longer be scheduled, effectively ending the loop
    while (true)
    {
        // Accept a new connection. If this function fails, terminating is the
        // best option, so we use a throwing function (i.e. we don't use asio::as_tuple
        //  or asio::redirect_error).
        asio::ip::tcp::socket sock = co_await acceptor.async_accept();

        // Launch a new session for this connection. Each session gets its
        // own coroutine, so we can get back to listening for new connections.
        asio::co_spawn(
            // Use the same executor as the current coroutine
            ex,

            // The function to run
            run_http_session(std::move(sock), st),

            // If an exception was thrown in a session, log it but don't propagate it.
            // This way, an unhandled error in a session affects only this session,
            // rather than bringing the entire server down
            [](std::exception_ptr exc) {
                if (exc)
                    log_exception(exc);
            }
        );
    }
}
