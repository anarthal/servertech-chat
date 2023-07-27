//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "redis_client.hpp"

#include <boost/asio/detached.hpp>

#include "error.hpp"

chat::redis_client::redis_client(boost::asio::any_io_executor ex) : conn_(ex) {}  //

void chat::redis_client::start_run()
{
    boost::redis::config cfg;
    cfg.health_check_interval = std::chrono::seconds::zero();
    conn_.async_run(cfg, {}, boost::asio::detached);
}

void chat::redis_client::cancel() { conn_.cancel(); }

chat::result<std::vector<std::vector<chat::message>>> chat::redis_client::get_room_history(
    boost::span<const room> rooms,
    boost::asio::yield_context yield
)
{
    assert(!rooms.empty());

    // Compose the request
    boost::redis::request req;
    req.push("XREVRANGE", rooms[0].id, "+", "-", "COUNT", message_batch_size);
    for (std::size_t i = 1; i < rooms.size(); ++i)
    {
        req.push("XREVRANGE", rooms[i].id, "+", "-", "COUNT", "1");
    }

    boost::redis::generic_response res;
    chat::error_code ec;

    conn_.async_exec(req, res, yield[ec]);

    if (ec)
        return ec;

    return chat::parse_room_history_batch(res);
}

chat::result<std::vector<chat::message>> chat::redis_client::get_room_history(
    std::string_view room_id,
    std::string_view last_message_id,
    boost::asio::yield_context yield
)
{
    // Compose the request. TODO: this yields a repeated message
    boost::redis::request req;
    req.push("XREVRANGE", room_id, last_message_id, "-", "COUNT", message_batch_size);

    // Execute it
    boost::redis::generic_response res;
    chat::error_code ec;
    conn_.async_exec(req, res, yield[ec]);
    if (ec)
        return ec;

    // Parse it
    return chat::parse_room_history(res);
}

chat::error_code chat::redis_client::store_messages(
    std::string_view room_id,
    boost::span<const message> messages,
    boost::asio::yield_context yield
)
{
    boost::redis::request req;
    error_code ec;

    // TODO: we should get IDs from here
    for (const auto& msg : messages)
        req.push("XADD", room_id, "*", "payload", chat::serialize_redis_message(msg));

    conn_.async_exec(req, boost::redis::ignore, yield[ec]);

    return ec;
}