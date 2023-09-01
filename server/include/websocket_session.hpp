//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_WEBSOCKET_SESSION_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_WEBSOCKET_SESSION_HPP

#include <boost/asio/spawn.hpp>

#include <memory>
#include <string>

#include "error.hpp"
#include "websocket.hpp"

namespace chat {

// Forward declaration
class shared_state;

class websocket_session : public std::enable_shared_from_this<websocket_session>
{
    websocket ws_;
    std::shared_ptr<shared_state> state_;

public:
    // Constructor. websocket::accept should have already been called
    websocket_session(websocket socket, std::shared_ptr<shared_state> state);

    // Runs the session until the client disconnects or an error occurs
    void run(boost::asio::yield_context yield);

    // Writes a message to the client. Used for message broadcasting
    error_code write(std::string_view msg, boost::asio::yield_context yield);
};

}  // namespace chat

#endif
