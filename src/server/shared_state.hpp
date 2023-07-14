//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SRC_SERVER_SHARED_STATE_HPP
#define SERVERTECHCHAT_SRC_SERVER_SHARED_STATE_HPP

#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

namespace chat {

// Forward declaration
class websocket_session;

// Represents the shared server state
class shared_state
{
    const std::string doc_root_;

    // This mutex synchronizes all access to sessions_
    std::mutex mutex_;

    // Keep a list of all the connected clients
    std::unordered_set<websocket_session*> sessions_;

public:
    explicit shared_state(std::string doc_root);

    std::string const& doc_root() const noexcept { return doc_root_; }

    void join(websocket_session* session);
    void leave(websocket_session* session);
    void send(std::string message);
};

}  // namespace chat

#endif
