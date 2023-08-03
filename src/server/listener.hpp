//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SRC_SERVER_LISTENER_HPP
#define SERVERTECHCHAT_SRC_SERVER_LISTENER_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <memory>

namespace chat {

// Forward declaration
class shared_state;

// Listener is run until the io_context is stopped, as a detached task
void run_listener(
    boost::asio::io_context& ctx,
    boost::asio::ip::tcp::endpoint listening_endpoint,
    std::shared_ptr<shared_state> state
);

}  // namespace chat

#endif
