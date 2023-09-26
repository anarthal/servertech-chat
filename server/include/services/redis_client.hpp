//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_REDIS_CLIENT_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_REDIS_CLIENT_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/core/span.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "business_types.hpp"
#include "error.hpp"

// A high-level, specialized Redis client. It implements the operations
// required by our server, abstracting away the actual Redis commands.

namespace chat {

// Using an interface to reduce build times and improve testability
class redis_client
{
public:
    virtual ~redis_client() {}

    // Starts the Redis runner task, in detached mode. This must be called once
    // to allow other operations to make progress and keep the reconnection loop
    // running
    virtual void start_run() = 0;

    // Cancels the Redis runner task. To be called at shutdown
    virtual void cancel() = 0;

    // The maximum number of messages for a room that get retrieved in a single go
    static constexpr std::size_t message_batch_size = 100;

    // Used as input parameter to get_room_history
    struct room_histoy_request
    {
        // The ID of the room we want to lookup
        std::string_view room_id;

        // The last message we have fo this room; leave empty for "from the latest"
        std::optional<std::string_view> last_message_id;
    };

    // Retrieves a batch of room history for several rooms
    virtual result_with_message<std::vector<message_batch>> get_room_history(
        boost::span<const room_histoy_request> reqs,
        boost::asio::yield_context yield
    ) = 0;

    // Inserts a batch of messages into a certain room's history.
    // Returns the IDs of the inserted messages
    virtual result_with_message<std::vector<std::string>> store_messages(
        std::string_view room_id,
        boost::span<const message> messages,
        boost::asio::yield_context yield
    ) = 0;

    // Sets a certain key to a value, with the given time to live.
    // If the key already exists, the operation fails with already_exists
    virtual error_with_message set_nonexisting_key(
        std::string_view key,
        std::string_view value,
        std::chrono::seconds ttl,
        boost::asio::yield_context yield
    ) = 0;

    // Gets the specified key, as an int64_t.
    // Returns not_found if the key does not exist
    virtual result_with_message<std::int64_t> get_int_key(
        std::string_view key,
        boost::asio::yield_context yield
    ) = 0;
};

// Creates a concrete implementation of redis_client
std::unique_ptr<redis_client> create_redis_client(boost::asio::any_io_executor ex);

}  // namespace chat

#endif
