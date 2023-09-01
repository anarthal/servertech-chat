//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SHARED_STATE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SHARED_STATE_HPP

#include <memory>
#include <string>

#include "redis_client.hpp"

namespace chat {

// Forward declaration
class session_map;

// Contains singleton objects shared by all sessions in the server
class shared_state
{
    // pimpl-like idiom to avoid including session_map.hpp
    struct
    {
        std::string doc_root_;
        redis_client redis_;
        std::unique_ptr<session_map> sessions_;
    } impl_;

public:
    shared_state(std::string doc_root, redis_client redis);
    shared_state(const shared_state&) = delete;
    shared_state(shared_state&&) noexcept;
    shared_state& operator=(const shared_state&) = delete;
    shared_state& operator=(shared_state&&) noexcept;
    ~shared_state();

    const std::string& doc_root() const noexcept { return impl_.doc_root_; }
    redis_client& redis() noexcept { return impl_.redis_; }
    session_map& sessions() noexcept { return *impl_.sessions_; }
};

}  // namespace chat

#endif
