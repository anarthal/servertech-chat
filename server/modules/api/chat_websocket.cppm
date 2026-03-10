//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

module;

#include <boost/asio/awaitable.hpp>
#include <boost/system/error_code.hpp>

module servertech_chat:chat_websocket;

import :websocket;
import std;

namespace chat {

// Forward declaration
class shared_state;

// Runs the chat websocket session until an error occurs.
boost::asio::awaitable<boost::system::error_code> handle_chat_websocket(
    websocket socket,
    std::shared_ptr<shared_state> state
);

}  // namespace chat
