//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "http_session.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket/rfc6455.hpp>

#include "error.hpp"
#include "serve_static_file.hpp"
#include "shared_state.hpp"
#include "websocket_session.hpp"

namespace beast = boost::beast;
namespace http = boost::beast::http;

void chat::run_http_session(
    boost::asio::ip::tcp::socket&& socket,
    std::shared_ptr<shared_state> state,
    boost::asio::yield_context yield
)
{
    error_code ec;

    // A buffer to read incoming client requests
    boost::beast::flat_buffer buff;

    // A stream allows us to set quality-of-service parameters for the connection,
    // like timeouts.
    boost::beast::tcp_stream stream(std::move(socket));

    while (true)
    {
        // Construct a new parser for each message
        boost::beast::http::request_parser<boost::beast::http::string_body> parser;

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser.body_limit(10000);

        // Set the timeout.
        stream.expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(stream, buff, parser.get(), yield[ec]);

        if (ec == http::error::end_of_stream)
        {
            // This means they closed the connection
            stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            return;
        }
        else if (ec)
        {
            // An unknown error happened
            return log_error(ec, "read");
        }

        // See if it is a WebSocket Upgrade
        if (boost::beast::websocket::is_upgrade(parser.get()))
        {
            // Create a websocket, transferring ownership of the socket
            // and the buffer (we're not using them again here)
            websocket ws(stream.release_socket(), std::move(buff));

            // Perform the session handshake
            ec = ws.accept(parser.release(), yield);
            if (ec)
                return log_error(ec, "websocket accept");

            // Create a websocket session object and run it until the client
            // closes the connection or an error occurs
            auto websocket_sess = std::make_shared<websocket_session>(std::move(ws), state);
            websocket_sess->run(yield);
            return;
        }

        // If it's not a websocket upgrade, it's a request for a static file.
        // Attempt to serve it and generate a response
        http::message_generator msg = serve_static_file(state->doc_root(), parser.release());

        // Determine if we should close the connection
        bool keep_alive = msg.keep_alive();

        // Send the response
        beast::async_write(stream, std::move(msg), yield[ec]);
        if (ec)
            return log_error(ec, "write");

        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        if (!keep_alive)
        {
            stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            return;
        }
    }
}
