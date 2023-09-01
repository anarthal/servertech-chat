//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_REDIS_CLIENT_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_REDIS_CLIENT_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/core/span.hpp>

#include <string_view>
#include <vector>

#include "business.hpp"
#include "error.hpp"

// A high-level, specialized Redis client. It implements the operations
// required b our server, abstracting away the actual Redis commands.

namespace chat {

class redis_client
{
    // Using the pimp idiom to reduce build times
    struct impl;
    std::unique_ptr<impl> impl_;

public:
    // Constructors, assignments, destructor
    redis_client(boost::asio::any_io_executor ex);
    redis_client(const redis_client&) = delete;
    redis_client(redis_client&&) noexcept;
    redis_client& operator=(const redis_client&) = delete;
    redis_client& operator=(redis_client&&) noexcept;
    ~redis_client();

    // Starts the Redis runner task, in detached mode. This must be called once
    // to allow other operations to make progress and keep the reconnection loop
    // running
    void start_run();

    // Cancels the Redis runner task. To be called at shutdown
    void cancel();

    // The maximum number of messages for a room that get retrieved in a single go
    static constexpr std::size_t message_batch_size = 100;

    // Retrieves a full batch of room history for several rooms
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

    // Inserts a batch of messages into a certain room's history.
    // Returns a list with IDs for the newly inserted messages
    result<std::vector<std::string>> store_messages(
        std::string_view room_id,
        boost::span<const message> messages,
        boost::asio::yield_context yield
    );
};

}  // namespace chat

#endif
