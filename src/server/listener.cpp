//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "listener.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>

#include <memory>

#include "http_session.hpp"
#include "shared_state.hpp"

static void fail(chat::error_code ec, char const* what, boost::asio::io_context& ctx)
{
    chat::log_error(ec, what);
    ctx.stop();
}

static void do_listen(
    boost::asio::ip::tcp::endpoint listening_endpoint,
    boost::asio::io_context& ctx,
    std::shared_ptr<chat::shared_state> st,
    boost::asio::yield_context yield
)
{
    boost::asio::ip::tcp::acceptor acceptor_(ctx.get_executor());
    chat::error_code ec;

    // Open the acceptor
    acceptor_.open(listening_endpoint.protocol(), ec);
    if (ec)
    {
        fail(ec, "open", ctx);
        return;
    }

    // Allow address reuse
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
    {
        fail(ec, "set_option", ctx);
        return;
    }

    // Bind to the server address
    acceptor_.bind(listening_endpoint, ec);
    if (ec)
    {
        fail(ec, "bind", ctx);
        return;
    }

    // Start listening for connections
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        fail(ec, "listen", ctx);
        return;
    }

    while (!ctx.stopped())
    {
        auto sock = acceptor_.async_accept(yield[ec]);
        if (ec)
            return chat::log_error(ec, "accept");

        // Launch a new session for this connection
        boost::asio::spawn(
            ctx.get_executor(),
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

void chat::run_listener(
    boost::asio::io_context& ctx,
    boost::asio::ip::tcp::endpoint listening_endpoint,
    std::shared_ptr<shared_state> state
)
{
    boost::asio::spawn(
        ctx.get_executor(),
        [listening_endpoint, &ctx, st = state](boost::asio::yield_context yield) mutable {
            do_listen(listening_endpoint, ctx, std::move(st), yield);
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
