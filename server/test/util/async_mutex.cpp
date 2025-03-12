//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/async_mutex.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <exception>

#include "error.hpp"

using namespace chat;
namespace asio = boost::asio;

static constexpr auto rethrow_on_error = [](std::exception_ptr ptr) {
    if (ptr)
        std::rethrow_exception(ptr);
};

// Spawns a coroutine and runs it until completion
static void run_coroutine(asio::awaitable<void> (*fn)())
{
    asio::io_context ctx;
    asio::co_spawn(ctx, fn, rethrow_on_error);
    ctx.run();
}

BOOST_AUTO_TEST_SUITE(async_mutex_)

BOOST_AUTO_TEST_CASE(lock)
{
    run_coroutine([]() -> asio::awaitable<void> {
        // I/O objects
        async_mutex mtx(co_await asio::this_coro::executor);

        // Lock
        co_await mtx.lock();
        BOOST_TEST(mtx.locked());

        // Unlock
        mtx.unlock();
        BOOST_TEST(!mtx.locked());
    });
}

BOOST_AUTO_TEST_CASE(lock_with_guard)
{
    run_coroutine([]() -> asio::awaitable<void> {
        // I/O objects
        async_mutex mtx(co_await asio::this_coro::executor);

        // Lock
        auto guard = co_await mtx.lock_with_guard();
        BOOST_TEST(mtx.locked());

        // Unlock
        guard.reset();
        BOOST_TEST(!mtx.locked());
    });
}

BOOST_AUTO_TEST_CASE(try_lock)
{
    run_coroutine([]() -> asio::awaitable<void> {
        // I/O objects
        async_mutex mtx(co_await asio::this_coro::executor);

        // Lock
        bool ok = mtx.try_lock();
        BOOST_TEST(ok);
        BOOST_TEST(mtx.locked());

        // Trying to lock a locked mutex fails
        ok = mtx.try_lock();
        BOOST_TEST(!ok);
        BOOST_TEST(mtx.locked());

        // Unlock
        mtx.unlock();
        BOOST_TEST(!mtx.locked());
    });
}

BOOST_AUTO_TEST_CASE(lock_contention)
{
    run_coroutine([]() -> asio::awaitable<void> {
        // I/O objects
        async_mutex mtx(co_await asio::this_coro::executor);
        asio::steady_timer timer(co_await asio::this_coro::executor);
        asio::experimental::channel<void(error_code)> chan(co_await asio::this_coro::executor, 1);

        // Lock the mutex
        auto guard = co_await mtx.lock_with_guard();
        BOOST_TEST(mtx.locked());

        // Launch another coroutine that tries to acquire it
        asio::co_spawn(
            co_await asio::this_coro::executor,
            [&]() -> asio::awaitable<void> {
                // Mutex should be held by the main coroutine
                BOOST_TEST_REQUIRE(mtx.locked());

                // Lock and unlock
                co_await mtx.lock();
                mtx.unlock();

                // Notify the main coroutine that we're done
                bool ok = chan.try_send(error_code());
                BOOST_TEST_REQUIRE(ok);
            },
            rethrow_on_error
        );

        // Yield so that the other coroutine tries to acquire the mutex while being held by us
        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait();

        // Unlock the mutex
        guard.reset();

        // Wait for the other coroutine to finish
        co_await chan.async_receive();

        BOOST_TEST(!mtx.locked());
    });
}

BOOST_AUTO_TEST_SUITE_END()
