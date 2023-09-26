//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/async_mutex.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <exception>
#include <functional>

#include "error.hpp"

using namespace chat;

// Spawns a coroutine, detached. Rethrows any exceptions
static void spawn_coroutine(
    boost::asio::any_io_executor ex,
    std::function<void(boost::asio::yield_context)> fn
)
{
    boost::asio::spawn(std::move(ex), std::move(fn), [](std::exception_ptr ptr) {
        if (ptr)
            std::rethrow_exception(ptr);
    });
}

// Spawns a coroutine and runs it until completion
static void run_coroutine(void (*fn)(boost::asio::yield_context))
{
    boost::asio::io_context ctx;
    spawn_coroutine(ctx.get_executor(), fn);
    ctx.run();
}

BOOST_AUTO_TEST_SUITE(async_mutex_)

BOOST_AUTO_TEST_CASE(lock)
{
    run_coroutine([](boost::asio::yield_context yield) {
        // I/O objects
        async_mutex mtx(yield.get_executor());

        // Lock
        mtx.lock(yield);
        BOOST_TEST(mtx.locked());

        // Unlock
        mtx.unlock();
        BOOST_TEST(!mtx.locked());
    });
}

BOOST_AUTO_TEST_CASE(lock_with_guard)
{
    run_coroutine([](boost::asio::yield_context yield) {
        // I/O objects
        async_mutex mtx(yield.get_executor());

        // Lock
        auto guard = mtx.lock_with_guard(yield);
        BOOST_TEST(mtx.locked());

        // Unlock
        guard.reset();
        BOOST_TEST(!mtx.locked());
    });
}

BOOST_AUTO_TEST_CASE(try_lock)
{
    run_coroutine([](boost::asio::yield_context yield) {
        // I/O objects
        async_mutex mtx(yield.get_executor());

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
    run_coroutine([](boost::asio::yield_context yield) {
        // I/O objects
        async_mutex mtx(yield.get_executor());
        boost::asio::steady_timer timer(yield.get_executor());
        boost::asio::experimental::channel<void(error_code)> chan(yield.get_executor(), 1);

        // Lock the mutex
        auto guard = mtx.lock_with_guard(yield);
        BOOST_TEST(mtx.locked());

        // Launch another coroutine that tries to acquire it
        spawn_coroutine(yield.get_executor(), [&](boost::asio::yield_context yield) {
            // Mutex should be held by the main coroutine
            BOOST_TEST_REQUIRE(mtx.locked());

            // Lock and unlock
            mtx.lock(yield);
            mtx.unlock();

            // Notify the main coroutine that we're done
            bool ok = chan.try_send(error_code());
            BOOST_TEST_REQUIRE(ok);
        });

        // Yield so that the other coroutine tries to acquire the mutex while being held by us
        timer.expires_after(std::chrono::milliseconds(10));
        timer.async_wait(yield);

        // Unlock the mutex
        guard.reset();

        // Wait for the other coroutine to finish
        chan.async_receive(yield);

        BOOST_TEST(!mtx.locked());
    });
}

BOOST_AUTO_TEST_SUITE_END()
