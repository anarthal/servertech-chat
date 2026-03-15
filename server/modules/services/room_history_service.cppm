//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

module servertech_chat:room_history_service;

import boost.asio;
import boost.system;
import :business_types;
import std;

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
    boost::asio::awaitable<boost::system::result<std::pair<std::vector<message_batch>, username_map>>> get_room_history(
        std::span<const std::string_view> room_ids
    );

    // Same as the above, but for an individual room.
    boost::asio::awaitable<boost::system::result<std::pair<message_batch, username_map>>> get_room_history(
        std::string_view room_id
    );
};

}  // namespace chat
