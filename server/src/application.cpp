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
#include <boost/system/errc.hpp>

#include <coroutine>

#include "error.hpp"
#include "listener.hpp"
#include "promise.hpp"
#include "redis_client.hpp"
#include "shared_state.hpp"

using namespace chat;

namespace {

class redis_runner
{
    std::shared_ptr<shared_state> st_;
    promise<error_code> runner_promise_;

public:
    redis_runner(std::shared_ptr<shared_state> st) : st_(std::move(st)), runner_promise_(st_->redis().run())
    {
    }

    promise<void> await_exit(std::exception_ptr)
    {
        st_->redis().cancel();
        auto ec = co_await runner_promise_;
        if (ec && ec != boost::system::errc::operation_canceled)
            log_error(ec, "Running Redis client");
    }
};

}  // namespace

promise<error_code> chat::run_application(const application_config& config)
{
    error_code ec;

    // Create objects
    boost::asio::ip::tcp::endpoint listening_endpoint{boost::asio::ip::make_address(config.ip), config.port};
    auto st = std::make_shared<shared_state>(
        std::move(config.doc_root),
        redis_client(co_await boost::async::this_coro::executor)
    );

    // Launch a Redis client in the background. Cancel it on exit
    co_await boost::async::with(redis_runner(st), [&](redis_runner&) -> promise<void> {
        // Run the listening loop
        ec = co_await run_listener(listening_endpoint, st);
    });

    co_return ec;
}
