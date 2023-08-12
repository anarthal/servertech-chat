//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "redis_client.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/async/op.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/response.hpp>

#include <cstdlib>
#include <utility>

#include "error.hpp"
#include "serialization.hpp"
#include "use_nothrow_op.hpp"

using namespace chat;

struct chat::redis_client::impl
{
    boost::redis::connection conn_;

    impl(boost::asio::any_io_executor ex) : conn_(ex) {}
};

chat::redis_client::redis_client(boost::asio::any_io_executor ex) : impl_(new impl(ex)) {}

chat::redis_client::redis_client(redis_client&& rhs) noexcept : impl_(std::move(rhs.impl_)) {}

chat::redis_client& chat::redis_client::operator=(redis_client&& rhs) noexcept
{
    impl_ = std::move(impl_);
    return *this;
}

chat::redis_client::~redis_client() {}

promise<error_code> chat::redis_client::run()
{
    const char* host_c_str = std::getenv("REDIS_HOST");
    std::string host = host_c_str ? host_c_str : "localhost";

    boost::redis::config cfg;
    cfg.addr.host = std::move(host);
    cfg.health_check_interval = std::chrono::seconds::zero();
    auto [ec] = co_await impl_->conn_.async_run(cfg, {}, use_nothrow_op);
    co_return ec;
}

void chat::redis_client::cancel() { impl_->conn_.cancel(); }

promise<result<redis_client::multiroom_history_t>> chat::redis_client::get_room_history(
    boost::span<const room> rooms
)
{
    assert(!rooms.empty());

    // Compose the request
    boost::redis::request req;
    for (const auto& room : rooms)
        req.push("XREVRANGE", room.id, "+", "-", "COUNT", message_batch_size);

    boost::redis::generic_response res;

    auto [ec, ign] = co_await impl_->conn_.async_exec(req, res, use_nothrow_op);
    boost::ignore_unused(ign);

    if (ec)
        co_return ec;

    co_return chat::parse_room_history_batch(res);
}

promise<result<std::vector<message>>> chat::redis_client::get_room_history(
    std::string_view room_id,
    std::string_view last_message_id
)
{
    // Compose the request. TODO: this yields a repeated message
    boost::redis::request req;
    req.push("XREVRANGE", room_id, last_message_id, "-", "COUNT", message_batch_size);

    // Execute it
    boost::redis::generic_response res;
    auto [ec, ign] = co_await impl_->conn_.async_exec(req, res, use_nothrow_op);
    boost::ignore_unused(ign);
    if (ec)
        co_return ec;

    // Parse it
    co_return chat::parse_room_history(res);
}

promise<result<std::vector<std::string>>> chat::redis_client::store_messages(
    std::string_view room_id,
    boost::span<const message> messages
)
{
    // Compose the request
    boost::redis::request req;
    for (const auto& msg : messages)
        req.push("XADD", room_id, "*", "payload", chat::serialize_redis_message(msg));

    // Execute it
    boost::redis::generic_response res;
    auto [ec, ign] = co_await impl_->conn_.async_exec(req, res, use_nothrow_op);
    boost::ignore_unused(ign);
    if (ec)
        co_return ec;

    // Parse the response
    co_return parse_string_list(res);
}
