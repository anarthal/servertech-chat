//
// Copyright (c) 2023-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_API_CHAT_WEBSOCKET_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_API_CHAT_WEBSOCKET_HPP

#include <boost/asio/awaitable.hpp>

#include <memory>

#include "error.hpp"
#include "util/websocket.hpp"

namespace chat {

// Forward declaration
class shared_state;

// Runs the chat websocket session until an error occurs.
boost::asio::awaitable<error_with_message> handle_chat_websocket(
    websocket socket,
    std::shared_ptr<shared_state> state
);

}  // namespace chat

#endif
