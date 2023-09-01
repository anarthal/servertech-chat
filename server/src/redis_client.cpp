//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "redis_client.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/response.hpp>

#include <cstdlib>
#include <utility>

#include "error.hpp"
#include "redis_serialization.hpp"

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

void chat::redis_client::start_run()
{
    // The host to connect to. Defaults to localhost
    const char* host_c_str = std::getenv("REDIS_HOST");
    std::string host = host_c_str ? host_c_str : "localhost";

    boost::redis::config cfg;
    cfg.addr.host = std::move(host);
    cfg.health_check_interval = std::chrono::seconds::zero();  // Disable health checks for now
    impl_->conn_.async_run(cfg, {}, boost::asio::detached);
}

void chat::redis_client::cancel() { impl_->conn_.cancel(); }

chat::result<std::vector<std::vector<chat::message>>> chat::redis_client::get_room_history(
    boost::span<const room> rooms,
    boost::asio::yield_context yield
)
{
    assert(!rooms.empty());

    // Compose the request. XREVRANGE will get all messages for a room,
    // since the beginning, in reverse order, up to message_batch_size
    // TODO: we could just retrieve the 1st message for all except the 1st room
    boost::redis::request req;
    for (const auto& room : rooms)
        req.push("XREVRANGE", room.id, "+", "-", "COUNT", message_batch_size);

    // Run it
    boost::redis::generic_response res;
    chat::error_code ec;
    impl_->conn_.async_exec(req, res, yield[ec]);
    if (ec)
        return ec;

    // Parse the response
    return chat::parse_room_history_batch(res);
}

chat::result<std::vector<chat::message>> chat::redis_client::get_room_history(
    std::string_view room_id,
    std::string_view last_message_id,
    boost::asio::yield_context yield
)
{
    // Compose the request. This will retrieve all messages for a room,
    // in reverse order, starting at last_message_id, up to message_batch_size
    // TODO: this yields a repeated message. We should discard the first one
    boost::redis::request req;
    req.push("XREVRANGE", room_id, last_message_id, "-", "COUNT", message_batch_size);

    // Execute it
    boost::redis::generic_response res;
    chat::error_code ec;
    impl_->conn_.async_exec(req, res, yield[ec]);
    if (ec)
        return ec;

    // Parse it
    return chat::parse_room_history(res);
}

chat::result<std::vector<std::string>> chat::redis_client::store_messages(
    std::string_view room_id,
    boost::span<const message> messages,
    boost::asio::yield_context yield
)
{
    // Compose the request. This appends a message to the given room and
    // auto-assigns it an ID.
    boost::redis::request req;
    for (const auto& msg : messages)
        req.push("XADD", room_id, "*", "payload", chat::serialize_redis_message(msg));

    // Execute it
    boost::redis::generic_response res;
    error_code ec;
    impl_->conn_.async_exec(req, res, yield[ec]);
    if (ec)
        return ec;

    // Parse the response
    return parse_batch_xadd_response(res);
}
