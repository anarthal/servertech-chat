//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_LISTENER_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_LISTENER_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <memory>

#include "error.hpp"

namespace chat {

// Forward declaration
class shared_state;

// Launchs a HTTP listener that will accept connections in a loop until
// the underlying I/O context is stopped. Returns a non-zero error_code
// if the listener was unable to launch (e.g. the port to bind to is not available).
error_code launch_http_listener(
    boost::asio::any_io_executor ex,
    boost::asio::ip::tcp::endpoint listening_endpoint,
    std::shared_ptr<shared_state> state
);

}  // namespace chat

#endif
