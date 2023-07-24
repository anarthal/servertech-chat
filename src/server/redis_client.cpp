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

chat::result<std::vector<chat::message>> chat::redis_client::get_room_history(
    std::string_view room_name,
    boost::asio::yield_context yield
)
{
    boost::redis::request req;
    boost::redis::generic_response res;
    chat::error_code ec;

    req.push("XREVRANGE", room_name, "+", "-");
    conn_.async_exec(req, res, yield[ec]);

    if (ec)
        return ec;

    return chat::parse_room_history(res);
}

chat::error_code chat::redis_client::add_message(
    std::string_view room_name,
    const message& msg,
    boost::asio::yield_context yield
)
{
    boost::redis::request req;
    error_code ec;

    req.push("XADD", room_name, "*", "payload", chat::serialize_redis_message(msg));

    conn_.async_exec(req, boost::redis::ignore, yield[ec]);

    return ec;
}