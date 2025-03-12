//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "listener.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/assert/source_location.hpp>

#include <memory>

#include "error.hpp"
#include "http_session.hpp"
#include "services/mysql_client.hpp"
#include "shared_state.hpp"

namespace asio = boost::asio;
using namespace chat;

// An exception handler for coroutines that rethrows any exception thrown by
// the coroutine. We handle all known error cases with error_code's. If an
// exception is raised, it's something critical, e.g. out of memory.
// Re-raising it in the exception handler will case io_context::run()
// to throw and terminate the program.
static constexpr auto rethrow_handler = [](std::exception_ptr ex) {
    if (ex)
        std::rethrow_exception(ex);
};

// The actual accept loop, coroutine-based
static asio::awaitable<void> accept_loop(
    asio::ip::tcp::acceptor acceptor,
    std::shared_ptr<chat::shared_state> st
)
{
    error_code ec;

    // Run DB setup code. If this fails is because something really bad happened
    // (e.g. the SQL execution failed), so we throw an exception. Network errors
    // are handled by setup_db.
    auto err = co_await st->mysql().setup_db();
    if (err.ec)
        throw_exception_from_error(err, BOOST_CURRENT_LOCATION);

    // We accept connections in an infinite loop. When the io_context is stopped,
    // coroutines are "cancelled" by throwing an internal exception, exiting
    // the loop.
    while (true)
    {
        // Accept a new connection
        auto [ec, sock] = co_await acceptor.async_accept(asio::as_tuple);
        if (ec)
        {
            log_error(ec, "accept");
            co_return;
        }

        // Launch a new session for this connection. Each session gets its
        // own coroutine, so we can get back to listening for new connections.
        asio::co_spawn(
            co_await asio::this_coro::executor,
            run_http_session(std::move(sock), st),
            rethrow_handler  // Propagate exceptions to the io_context
        );
    }
}

error_code chat::launch_http_listener(
    asio::any_io_executor ex,
    asio::ip::tcp::endpoint listening_endpoint,
    std::shared_ptr<shared_state> state
)
{
    error_code ec;

    // An object that allows us to acept incoming TCP connections
    asio::ip::tcp::acceptor acceptor{ex};

    // Open the acceptor
    acceptor.open(listening_endpoint.protocol(), ec);
    if (ec)
        return ec;

    // Allow address reuse
    acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec)
        return ec;

    // Bind to the server address
    acceptor.bind(listening_endpoint, ec);
    if (ec)
        return ec;

    // Start listening for connections
    acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec)
        return ec;

    // Spawn a coroutine that will accept the connections. From this point,
    // everything is handled asynchronously, with stackful coroutines.
    asio::co_spawn(
        std::move(ex),
        accept_loop(std::move(acceptor), std::move(state)),
        rethrow_handler  // Propagate exceptions to the io_context
    );

    return error_code();
}
