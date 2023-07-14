//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "websocket_session.hpp"

#include <boost/beast/core/buffers_to_string.hpp>

#include <iostream>

#include "shared_state.hpp"

namespace net = boost::asio;
namespace websocket = boost::beast::websocket;

static void fail(chat::error_code ec, char const* what)
{
    // Don't report these
    if (ec == net::error::operation_aborted || ec == websocket::error::closed)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}

chat::websocket_session::websocket_session(net::ip::tcp::socket&& socket, std::shared_ptr<shared_state> state)
    : ws_(std::move(socket)), state_(std::move(state))
{
}

chat::websocket_session::~websocket_session()
{
    // Remove this session from the list of active sessions
    state_->leave(this);
}

void chat::websocket_session::run(
    boost::beast::http::request<boost::beast::http::string_body> request,
    boost::asio::yield_context yield
)
{
    error_code ec;

    // Set suggested timeout settings for the websocket
    ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws_.set_option(
        boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::response_type& res) {
            res.set(
                boost::beast::http::field::server,
                std::string(BOOST_BEAST_VERSION_STRING) + " websocket-chat-multi"
            );
        })
    );

    // Accept the websocket handshake
    ws_.async_accept(request, yield[ec]);
    if (ec)
        return fail(ec, "accept");

    // Add this session to the list of active sessions
    state_->join(this);

    while (true)
    {
        // Read a message
        ws_.async_read(buffer_, yield[ec]);
        if (ec)
            return fail(ec, "read");

        // Send to all connections
        state_->send(boost::beast::buffers_to_string(buffer_.data()));

        // Clear the buffer
        buffer_.consume(buffer_.size());
    }
}

void chat::websocket_session::send(const boost::shared_ptr<const std::string>& ss)
{
    // Post our work to the strand, this ensures
    // that the members of `this` will not be
    // accessed concurrently.

    net::post(
        ws_.get_executor(),
        boost::beast::bind_front_handler(&websocket_session::on_send, shared_from_this(), ss)
    );
}

void chat::websocket_session::on_send(const boost::shared_ptr<const std::string>& ss)
{
    // Always add to queue
    queue_.push_back(ss);

    // Are we already writing?
    if (queue_.size() > 1)
        return;

    // We are not currently writing, so send this immediately
    ws_.async_write(
        net::buffer(*queue_.front()),
        boost::beast::bind_front_handler(&websocket_session::on_write, shared_from_this())
    );
}

void chat::websocket_session::on_write(error_code ec, std::size_t)
{
    // Handle the error, if any
    if (ec)
        return fail(ec, "write");

    // Remove the string from the queue
    queue_.erase(queue_.begin());

    // Send the next message if any
    if (!queue_.empty())
        ws_.async_write(
            net::buffer(*queue_.front()),
            boost::beast::bind_front_handler(&websocket_session::on_write, shared_from_this())
        );
}
