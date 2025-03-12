//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "http_session.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/variant2/variant.hpp>

#include <exception>
#include <string_view>
#include <utility>

#include "api/auth.hpp"
#include "api/chat_websocket.hpp"
#include "error.hpp"
#include "request_context.hpp"
#include "shared_state.hpp"
#include "static_files.hpp"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace asio = boost::asio;
using namespace chat;

static asio::awaitable<http::message_generator> handle_http_request_impl(
    request_context& ctx,
    shared_state& st
)
{
    // Attempt to parse the request target
    auto ec = ctx.parse_request_target();
    if (ec)
        co_return ctx.response().bad_request_text("Invalid request target");
    auto target = ctx.request_target();

    auto segs = target.segments();
    auto method = ctx.request_method();
    if (!segs.empty() && segs.front() == "api")
    {
        // API endpoint. All endpoint handlers have the signature
        // asio::awaitable<http::message_generator>(request_context&, shared_state&)
        auto it = std::next(segs.begin());
        auto seg = *it;
        ++it;
        if (seg == "create-account" && it == segs.end())
        {
            if (method == http::verb::post)
                co_return co_await handle_create_account(ctx, st);
            else
                co_return ctx.response().method_not_allowed();
        }
        else if (seg == "login" && it == segs.end())
        {
            if (method == http::verb::post)
                co_return co_await handle_login(ctx, st);
            else
                co_return ctx.response().method_not_allowed();
        }
        else
        {
            co_return ctx.response().not_found_text();
        }
    }
    else
    {
        // Static file
        co_return handle_static_file(ctx, st);
    }
}

static asio::awaitable<http::message_generator> handle_http_request(
    http::request<http::string_body>&& req,
    shared_state& st
)
{
    // Build a request context
    request_context ctx(std::move(req));

    // We don't communicate regular failures using exceptions, but
    // unhandled exceptions shouldn't crash the server.
    try
    {
        co_return co_await handle_http_request_impl(ctx, st);
    }
    catch (const std::exception& err)
    {
        co_return ctx.response().internal_server_error(errc::uncaught_exception, err.what());
    }
}

asio::awaitable<void> chat::run_http_session(
    boost::asio::ip::tcp::socket&& socket,
    std::shared_ptr<shared_state> state
)
{
    error_code ec;

    // A buffer to read incoming client requests
    beast::flat_buffer buff;

    // A stream allows us to set quality-of-service parameters for the connection,
    // like timeouts.
    beast::tcp_stream stream(std::move(socket));

    while (true)
    {
        // Construct a new parser for each message
        http::request_parser<http::string_body> parser;

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser.body_limit(10000);

        // Set the timeout.
        stream.expires_after(std::chrono::seconds(30));

        // Read a request
        co_await http::async_read(stream, buff, parser.get(), asio::redirect_error(ec));

        if (ec == http::error::end_of_stream)
        {
            // This means they closed the connection
            stream.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
            co_return;
        }
        else if (ec)
        {
            // An unknown error happened
            co_return log_error(ec, "read");
        }

        // See if it is a WebSocket Upgrade
        if (beast::websocket::is_upgrade(parser.get()))
        {
            // Create a websocket, transferring ownership of the socket
            // and the buffer (we're not using them again here)
            websocket ws(stream.release_socket(), parser.release(), std::move(buff));

            // Perform the session handshake
            ec = co_await ws.accept();
            if (ec)
            {
                log_error(ec, "websocket accept");
                co_return;
            }

            // Run the websocket session. This will run until the client
            // closes the connection or an error occurs.
            // We don't use exceptions to communicate regular failures, but an
            // unhandled exception in a websocket session shoudn't crash the server.
            try
            {
                auto err = co_await handle_chat_websocket(std::move(ws), state);
                if (err.ec && err.ec != beast::websocket::error::closed)
                    log_error(err, "Running chat websocket session");
            }
            catch (const std::exception& err)
            {
                log_error(
                    errc::uncaught_exception,
                    "Uncaught exception while running websocket session",
                    err.what()
                );
            }
            co_return;
        }

        // It's a regular HTTP request.
        // Attempt to serve it and generate a response
        http::message_generator msg = co_await handle_http_request(parser.release(), *state);

        // Determine if we should close the connection
        bool keep_alive = msg.keep_alive();

        // Send the response
        co_await beast::async_write(stream, std::move(msg), asio::redirect_error(ec));
        if (ec)
        {
            log_error(ec, "write");
            co_return;
        }

        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        if (!keep_alive)
        {
            stream.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
            co_return;
        }
    }
}
