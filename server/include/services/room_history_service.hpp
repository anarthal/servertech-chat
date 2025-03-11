//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_ROOM_HISTORY_SERVICE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_ROOM_HISTORY_SERVICE_HPP

#include <boost/asio/awaitable.hpp>

#include <span>
#include <string_view>
#include <utility>

#include "business_types.hpp"
#include "error.hpp"

// Contains functions to retrieve room chat history

namespace chat {

class mysql_client;
class redis_client;

class room_history_service
{
    redis_client* redis_;
    mysql_client* mysql_;

public:
    room_history_service(redis_client& redis, mysql_client& mysql) noexcept : redis_(&redis), mysql_(&mysql)
    {
    }

    // Retrieves room history for a batch of rooms. The returned vector
    // will have an entry per element in room_ids.
    // It also returns a (user_id, username) map containing entries for each user
    // that appears in the retrieved history.
    // If a room_id doesn't exist, an empty message_batch is returned for the room.
    // If a user_id is referenced in a message but doesn't exist, the entry
    // is not included in the map
    boost::asio::awaitable<result_with_message<std::pair<std::vector<message_batch>, username_map>>> get_room_history(
        std::span<const std::string_view> room_ids
    );

    // Same as the above, but for an individual room.
    boost::asio::awaitable<result_with_message<std::pair<message_batch, username_map>>> get_room_history(
        std::string_view room_id
    );
};

}  // namespace chat

#endif
