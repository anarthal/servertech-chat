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

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>

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
class websocket_session : public boost::enable_shared_from_this<websocket_session>
{
    boost::beast::flat_buffer buffer_;
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
    boost::shared_ptr<shared_state> state_;
    std::vector<boost::shared_ptr<std::string const>> queue_;

    void fail(error_code ec, char const* what);
    void on_accept(error_code ec);
    void on_read(error_code ec, std::size_t bytes_transferred);
    void on_write(error_code ec, std::size_t bytes_transferred);

public:
    websocket_session(boost::asio::ip::tcp::socket&& socket, const boost::shared_ptr<shared_state>& state);

    ~websocket_session();

    template <class Body, class Allocator>
    void run(boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>> req);

    // Send a message
    void send(const boost::shared_ptr<const std::string>& ss);

private:
    void on_send(const boost::shared_ptr<const std::string>& ss);
};

template <class Body, class Allocator>
void websocket_session::run(boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>> req
)
{
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
    ws_.async_accept(
        req,
        boost::beast::bind_front_handler(&websocket_session::on_accept, shared_from_this())
    );
}

}  // namespace chat

#endif
