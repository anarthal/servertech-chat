//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SRC_SERVER_WEBSOCKET_SESSION_HPP
#define SERVERTECHCHAT_SRC_SERVER_WEBSOCKET_SESSION_HPP

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

using ws_stream = boost::beast::websocket::stream<boost::beast::tcp_stream>;

class websocket_session : public std::enable_shared_from_this<websocket_session>
{
    boost::beast::flat_buffer buffer_;
    ws_stream ws_;
    std::shared_ptr<shared_state> state_;
    std::vector<boost::shared_ptr<const std::string>> queue_;
    std::string current_room_;

public:
    websocket_session(boost::asio::ip::tcp::socket&& socket, std::shared_ptr<shared_state> state);

    void run(
        boost::beast::http::request<boost::beast::http::string_body> request,
        boost::asio::yield_context yield
    );

    ws_stream& stream() noexcept { return ws_; }
};

}  // namespace chat

#endif
