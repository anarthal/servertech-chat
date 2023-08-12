//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_REDIS_CLIENT_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_REDIS_CLIENT_HPP

#include <boost/core/span.hpp>

#include <string_view>
#include <vector>

#include "error.hpp"
#include "promise.hpp"
#include "serialization.hpp"

namespace chat {

class redis_client
{
    struct impl;
    std::unique_ptr<impl> impl_;

public:
    redis_client(boost::asio::any_io_executor ex);
    redis_client(const redis_client&) = delete;
    redis_client(redis_client&&) noexcept;
    redis_client& operator=(const redis_client&) = delete;
    redis_client& operator=(redis_client&&) noexcept;
    ~redis_client();

    promise<error_code> run();
    void cancel();

    // The maximum number of messages for a room that get retrieved in a single go
    static constexpr std::size_t message_batch_size = 100;

    // Retrieves a full batch of room history for several rooms
    // TODO: we could just retrieve the 1st message for all except the 1st room
    using multiroom_history_t = std::vector<std::vector<message>>;
    promise<result<multiroom_history_t>> get_room_history(boost::span<const room> rooms);

    // Retrieves a batch of room history for a certain room, starting on a given message ID
    promise<result<std::vector<message>>> get_room_history(
        std::string_view room_id,
        std::string_view last_message_id
    );

    // Returns a list with IDs for the newly inserted messages
    promise<result<std::vector<std::string>>> store_messages(
        std::string_view room_id,
        boost::span<const message> messages
    );
};

}  // namespace chat

#endif
