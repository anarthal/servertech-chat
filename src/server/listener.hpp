//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SRC_SERVER_LISTENER_HPP
#define SERVERTECHCHAT_SRC_SERVER_LISTENER_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>

#include <memory>
#include <string>

#include "error.hpp"

namespace chat {

// Forward declaration
class shared_state;

// Accepts incoming connections and launches the sessions
class listener
{
    boost::asio::io_context& ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<shared_state> state_;

    void on_accept(error_code ec, boost::asio::ip::tcp::socket socket);
    void run_session(boost::asio::ip::tcp::socket socket, boost::asio::yield_context yield);
    void launch_listener();

public:
    listener(
        boost::asio::io_context& ioc,
        boost::asio::ip::tcp::endpoint endpoint,
        std::shared_ptr<shared_state> state
    );

    // Start accepting incoming connections
    void start();
};

}  // namespace chat

#endif
