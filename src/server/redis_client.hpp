//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SRC_SERVER_REDIS_CLIENT_HPP
#define SERVERTECHCHAT_SRC_SERVER_REDIS_CLIENT_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/core/span.hpp>
#include <boost/redis/connection.hpp>

#include <string_view>
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

    // The maximum number of messages for a room that get retrieved in a single go
    static constexpr std::size_t message_batch_size = 100;

    // Retrieves a full batch of room history for several rooms
    // TODO: we could just retrieve the 1st message for all except the 1st room
    result<std::vector<std::vector<message>>> get_room_history(
        boost::span<const room> rooms,
        boost::asio::yield_context yield
    );

    // Retrieves a batch of room history for a certain room, starting on a given message ID
    result<std::vector<message>> get_room_history(
        std::string_view room_id,
        std::string_view last_message_id,
        boost::asio::yield_context yield
    );

    // Returns a list with IDs for the newly inserted messages
    result<std::vector<std::string>> store_messages(
        std::string_view room_id,
        boost::span<const message> messages,
        boost::asio::yield_context yield
    );

    boost::asio::any_io_executor get_executor() { return conn_.get_executor(); }
};

}  // namespace chat

#endif
