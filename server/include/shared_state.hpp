//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SHARED_STATE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SHARED_STATE_HPP

#include <boost/asio/any_io_executor.hpp>

#include <memory>
#include <string>

namespace chat {

// Forward declaration
class session_map;
class redis_client;
class mysql_client;
class cookie_auth_service;
class pubsub_service;

// Contains singleton objects shared by all sessions in the server
class shared_state
{
    struct
    {
        std::string doc_root_;
        std::unique_ptr<redis_client> redis_;
        std::unique_ptr<mysql_client> mysql_;
        std::unique_ptr<cookie_auth_service> cookie_auth_;
        std::unique_ptr<pubsub_service> pubsub_;
    } impl_;

public:
    shared_state(std::string doc_root, boost::asio::any_io_executor ex);
    shared_state(const shared_state&) = delete;
    shared_state(shared_state&&) noexcept;
    shared_state& operator=(const shared_state&) = delete;
    shared_state& operator=(shared_state&&) noexcept;
    ~shared_state();

    const std::string& doc_root() const noexcept { return impl_.doc_root_; }
    redis_client& redis() noexcept { return *impl_.redis_; }
    mysql_client& mysql() noexcept { return *impl_.mysql_; }
    cookie_auth_service& cookie_auth() noexcept { return *impl_.cookie_auth_; }
    pubsub_service& pubsub() noexcept { return *impl_.pubsub_; }
};

}  // namespace chat

#endif
