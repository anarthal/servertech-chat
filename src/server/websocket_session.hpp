//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_WEBSOCKET_SESSION_HPP
#define BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_WEBSOCKET_SESSION_HPP

#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "error.hpp"

namespace chat {

// Forward declaration
class shared_state;

/** Represents an active WebSocket connection to the server
 */
class websocket_session : public std::enable_shared_from_this<websocket_session>
{
    boost::beast::flat_buffer buffer_;
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
    std::shared_ptr<shared_state> state_;
    std::vector<boost::shared_ptr<const std::string>> queue_;

    void on_write(error_code ec, std::size_t bytes_transferred);
    void on_send(const boost::shared_ptr<const std::string>& ss);

public:
    websocket_session(boost::asio::ip::tcp::socket&& socket, std::shared_ptr<shared_state> state);

    ~websocket_session();

    void run(
        boost::beast::http::request<boost::beast::http::string_body> request,
        boost::asio::yield_context yield
    );

    // Send a message
    void send(const boost::shared_ptr<const std::string>& ss);
};

}  // namespace chat

#endif
