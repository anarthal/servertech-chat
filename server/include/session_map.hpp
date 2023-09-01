//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SESSION_MAP_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SESSION_MAP_HPP

#include <boost/core/span.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <memory>
#include <string>

#include "business.hpp"

namespace chat {

// Forward declaration
class websocket_session;

// A data structure to broadcast messages to sessions. This is a multimap-like
// container mapping room IDs to websocket sessions.
// When a session is added to the map, it passes an array of room IDs to subscribe to.
// We can retrieve the sessions subscribed to a room by room ID.
// We can also remove a certain session by identity (used on session cleanup).
class session_map
{
    // The type of elements held by our container
    struct room_session
    {
        std::string room_id;
        std::shared_ptr<websocket_session> session;

        const websocket_session* session_ptr() const noexcept { return session.get(); }
    };

    // We need to efficiently index our container by both room ID and session
    // identity. We use a Boost.MultiIndex container to maintain such indices,
    // so that both operations run in logarithmic time.
    // clang-format off
    using container_type = boost::multi_index::multi_index_container<
        room_session,
        boost::multi_index::indexed_by<
            // Index by room ID
            boost::multi_index::ordered_non_unique<
                boost::multi_index::member<room_session, std::string, &room_session::room_id>
            >,
            // Index by session identity (comparing websocket_session pointers)
            boost::multi_index::ordered_non_unique<
                boost::multi_index::const_mem_fun<room_session, const websocket_session*, &room_session::session_ptr>
            >
        >
    >;
    // clang-format on

    container_type ct_;

    struct session_map_deleter
    {
        chat::session_map& self;

        void operator()(chat::websocket_session* sess) const noexcept { self.remove_session(*sess); }
    };

public:
    session_map() = default;

    // Subcribes a session to a list of rooms
    void add_session(std::shared_ptr<websocket_session> sess, boost::span<const room> rooms)
    {
        for (const auto& room : rooms)
        {
            ct_.insert(room_session{room.id, sess});
        }
    }

    // Removes a session (by identity)
    void remove_session(websocket_session& sess) noexcept { ct_.get<1>().erase(&sess); }

    // Gets all sessions subscribed to a certain room ID
    auto get_sessions(const std::string& room_id) { return ct_.equal_range(room_id); }

    // RAII-style add_session
    using session_map_guard = std::unique_ptr<chat::websocket_session, session_map_deleter>;
    session_map_guard add_session_guarded(
        std::shared_ptr<websocket_session> sess,
        boost::span<const room> rooms
    )
    {
        websocket_session* sessptr = sess.get();
        add_session(std::move(sess), rooms);
        return session_map_guard(sessptr, session_map_deleter{*this});
    }
};

}  // namespace chat

#endif
