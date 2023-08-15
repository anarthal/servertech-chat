//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SHARED_STATE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SHARED_STATE_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/core/span.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <memory>
#include <string>
#include <string_view>

#include "redis_client.hpp"

namespace chat {

// Forward declaration
class websocket_session;

class session_map
{
    struct room_session
    {
        std::string room_id;
        std::shared_ptr<websocket_session> session;

        const websocket_session* session_ptr() const noexcept { return session.get(); }
    };

    // clang-format off
    using container_type = boost::multi_index::multi_index_container<
        room_session,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_non_unique<
                boost::multi_index::member<room_session, std::string, &room_session::room_id>
            >,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::const_mem_fun<room_session, const websocket_session*, &room_session::session_ptr>
            >
        >
    >;
    // clang-format on

    container_type ct_;

public:
    session_map() = default;

    struct session_map_deleter
    {
        chat::session_map* sessmap{};

        void operator()(chat::websocket_session* sess) const noexcept
        {
            if (sessmap)
                sessmap->remove_session(*sess);
        }
    };

    using session_map_guard = std::unique_ptr<chat::websocket_session, session_map_deleter>;

    void add_session(std::shared_ptr<websocket_session> sess, boost::span<const room> rooms)
    {
        for (const auto& room : rooms)
        {
            ct_.insert(room_session{room.id, sess});
        }
    }

    void remove_session(websocket_session& sess) noexcept { ct_.get<1>().erase(&sess); }

    session_map_guard add_session_guarded(
        std::shared_ptr<websocket_session> sess,
        boost::span<const room> rooms
    )
    {
        add_session(sess, rooms);
        return session_map_guard{sess.get(), session_map_deleter{this}};
    }

    // TODO: we could save a copy here
    auto get_sessions(const std::string& room) { return ct_.equal_range(room); }
};

// Represents the shared server state
class shared_state
{
    const std::string doc_root_;
    redis_client redis_;
    session_map sessions_;

public:
    shared_state(std::string doc_root, redis_client redis);

    const std::string& doc_root() const noexcept { return doc_root_; }
    redis_client& redis() noexcept { return redis_; }
    session_map& sessions() noexcept { return sessions_; }
};

}  // namespace chat

#endif
