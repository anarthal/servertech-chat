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

    void add_session(std::shared_ptr<websocket_session> sess, boost::span<const room> rooms)
    {
        for (const auto& room : rooms)
        {
            ct_.insert(room_session{room.id, sess});
        }
    }

    void remove_session(websocket_session& sess) noexcept { ct_.get<1>().erase(&sess); }

    // TODO: we could save a copy here
    auto get_sessions(const std::string& room) { return ct_.equal_range(room); }
};

}  // namespace chat

#endif
