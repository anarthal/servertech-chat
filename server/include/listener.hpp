//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_LISTENER_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_LISTENER_HPP

#include <boost/asio/ip/tcp.hpp>

#include <memory>

#include "error.hpp"
#include "promise.hpp"

namespace chat {

// Forward declaration
class shared_state;

promise<error_code> run_listener(
    boost::asio::ip::tcp::endpoint listening_endpoint,
    std::shared_ptr<shared_state> state
);

}  // namespace chat

#endif
