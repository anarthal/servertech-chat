//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "listener.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/assert/source_location.hpp>

#include <memory>

#include "error.hpp"
#include "http_session.hpp"
#include "services/mysql_client.hpp"
#include "shared_state.hpp"

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
static void accept_loop(
    boost::asio::ip::tcp::acceptor acceptor,
    std::shared_ptr<chat::shared_state> st,
    boost::asio::yield_context yield
)
{
    error_code ec;

    // Run DB setup code. If this fails is because something really bad happened
    // (e.g. the SQL execution failed), so we throw an exception. Network errors
    // are handled by setup_db.
    auto err = st->mysql().setup_db(yield);
    if (err.ec)
        throw_exception_from_error(err, BOOST_CURRENT_LOCATION);

    // We accept connections in an infinite loop. When the io_context is stopped,
    // coroutines are "cancelled" by throwing an internal exception, exiting
    // the loop.
    while (true)
    {
        // Accept a new connection
        auto sock = acceptor.async_accept(yield[ec]);
        if (ec)
            return chat::log_error(ec, "accept");

        // Launch a new session for this connection. Each session gets its
        // own stackful coroutine, so we can get back to listening for new connections.
        boost::asio::spawn(
            sock.get_executor(),
            [state = st, socket = std::move(sock)](boost::asio::yield_context yield) mutable {
                run_http_session(std::move(socket), std::move(state), yield);
            },
            rethrow_handler  // Propagate exceptions to the io_context
        );
    }
}

error_code chat::launch_http_listener(
    boost::asio::any_io_executor ex,
    boost::asio::ip::tcp::endpoint listening_endpoint,
    std::shared_ptr<shared_state> state
)
{
    error_code ec;

    // An object that allows us to acept incoming TCP connections
    boost::asio::ip::tcp::acceptor acceptor{ex};

    // Open the acceptor
    acceptor.open(listening_endpoint.protocol(), ec);
    if (ec)
        return ec;

    // Allow address reuse
    acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
        return ec;

    // Bind to the server address
    acceptor.bind(listening_endpoint, ec);
    if (ec)
        return ec;

    // Start listening for connections
    acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
        return ec;

    // Spawn a coroutine that will accept the connections. From this point,
    // everything is handled asynchronously, with stackful coroutines.
    boost::asio::spawn(
        std::move(ex),
        [acceptor = std::move(acceptor), st = std::move(state)](boost::asio::yield_context yield) mutable {
            accept_loop(std::move(acceptor), std::move(st), yield);
        },
        rethrow_handler  // Propagate exceptions to the io_context
    );

    return error_code();
}
