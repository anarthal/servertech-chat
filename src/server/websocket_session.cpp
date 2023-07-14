//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#include "websocket_session.hpp"

#include <boost/beast/core/buffers_to_string.hpp>

#include <iostream>

#include "shared_state.hpp"

namespace net = boost::asio;
namespace websocket = boost::beast::websocket;

chat::websocket_session::websocket_session(
    net::ip::tcp::socket&& socket,
    const boost::shared_ptr<shared_state>& state
)
    : ws_(std::move(socket)), state_(state)
{
}

chat::websocket_session::~websocket_session()
{
    // Remove this session from the list of active sessions
    state_->leave(this);
}

void chat::websocket_session::fail(error_code ec, char const* what)
{
    // Don't report these
    if (ec == net::error::operation_aborted || ec == websocket::error::closed)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}

void chat::websocket_session::on_accept(error_code ec)
{
    // Handle the error, if any
    if (ec)
        return fail(ec, "accept");

    // Add this session to the list of active sessions
    state_->join(this);

    // Read a message
    ws_.async_read(
        buffer_,
        boost::beast::bind_front_handler(&websocket_session::on_read, shared_from_this())
    );
}

void chat::websocket_session::on_read(error_code ec, std::size_t)
{
    // Handle the error, if any
    if (ec)
        return fail(ec, "read");

    // Send to all connections
    state_->send(boost::beast::buffers_to_string(buffer_.data()));

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Read another message
    ws_.async_read(
        buffer_,
        boost::beast::bind_front_handler(&websocket_session::on_read, shared_from_this())
    );
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

void chat::websocket_session::on_send(boost::shared_ptr<std::string const> const& ss)
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
