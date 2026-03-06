//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_LISTENER_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_LISTENER_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <memory>

namespace chat {

// Forward declaration
class shared_state;

// Runs the HTTP server. It will accept connections in a loop until
// the underlying I/O context is stopped. Throws an exception
// if the listener is unable to launch (e.g. the port to bind to is not available).
boost::asio::awaitable<void> run_server(
    boost::asio::ip::tcp::endpoint listening_endpoint,
    std::shared_ptr<shared_state> state
);

}  // namespace chat

#endif
