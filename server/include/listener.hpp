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

class listener
{
    boost::asio::ip::tcp::acceptor acceptor_;

public:
    listener(boost::asio::any_io_executor ex);
    error_code setup(boost::asio::ip::tcp::endpoint listening_endpoint);
    void run_until_completion(std::shared_ptr<shared_state> state);
};

}  // namespace chat

#endif
