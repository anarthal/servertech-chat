//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "application.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/async/with.hpp>

#include "error.hpp"
#include "listener.hpp"
#include "redis_client.hpp"
#include "shared_state.hpp"

using namespace chat;

namespace {

struct redis_deleter
{
    void operator()(redis_client* cli) const noexcept { cli->cancel(); }
};
using redis_guard = std::unique_ptr<redis_client, redis_deleter>;

}  // namespace

promise<error_code> chat::run_application(const application_config& config)
{
    // Create objects
    boost::asio::ip::tcp::endpoint listening_endpoint{boost::asio::ip::make_address(config.ip), config.port};
    auto st = std::make_shared<shared_state>(
        std::move(config.doc_root),
        redis_client(co_await boost::async::this_coro::executor)
    );

    // Launch a Redis client in the background. Cancel it on exit
    +st->redis().run();
    redis_guard guard{&st->redis()};

    // Run the listening loop
    co_return co_await run_listener(listening_endpoint, st);
}
