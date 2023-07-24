//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SRC_SERVER_REDIS_HPP
#define SERVERTECHCHAT_SRC_SERVER_REDIS_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/redis/connection.hpp>

#include <vector>

#include "error.hpp"
#include "serialization.hpp"

namespace chat {

class redis_client
{
    boost::redis::connection conn_;

public:
    redis_client(boost::asio::any_io_executor ex);

    void start_run();
    void cancel();

    result<std::vector<message>> get_room_history(
        std::string_view room_name,
        boost::asio::yield_context yield
    );

    error_code add_message(std::string_view room_name, const message& msg, boost::asio::yield_context yield);

    boost::asio::any_io_executor get_executor() { return conn_.get_executor(); }
};

}  // namespace chat

#endif
