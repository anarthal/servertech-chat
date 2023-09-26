//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_ASYNC_MUTEX_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_ASYNC_MUTEX_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/spawn.hpp>

#include <cassert>
#include <deque>
#include <memory>

#include "error.hpp"

namespace chat {

// An asynchronous mutex to guarantee mutual exclusion in async code. This is
// similar to Python's asyncio.Mutex. Note that this is not thread-safe - it
// ensures mutual exclusion between coroutines.
class async_mutex
{
    // Is the mutex locked?
    bool locked_{false};

    // Acts as a condition variable, so that coroutines waiting to acquire
    // the mutex can be notified when another coroutine releases it
    boost::asio::experimental::channel<void(error_code)> chan_;

    struct guard_deleter
    {
        void operator()(async_mutex* self) const noexcept { self->unlock(); }
    };

public:
    // Constructors, assignments, destructor
    async_mutex(boost::asio::any_io_executor ex) : chan_(std::move(ex)) {}
    async_mutex(const async_mutex&) = delete;
    async_mutex(async_mutex&&) = default;
    async_mutex& operator=(const async_mutex&) = delete;
    async_mutex& operator=(async_mutex&&) = default;
    ~async_mutex() = default;

    // Is the mutex locked?
    bool locked() const noexcept { return locked_; }

    // Suspends the current coroutine until the mutex can be acquired, then acquire it
    void lock(boost::asio::yield_context yield)
    {
        // Most of the time this loop will be executed zero times (if unlocked)
        // or once (if locked). Race conditions could make another coroutine, different
        // from the one waked by async_receive, acquire the lock before it. This loop guards against it.
        while (locked_)
        {
            // Wait to be notified
            error_code ec;
            chan_.async_receive(yield[ec]);
            assert(!ec);
        }

        // Mark as locked
        locked_ = true;
    }

    // Try to acquire without suspending
    bool try_lock() noexcept
    {
        if (locked_)
            return false;
        locked_ = true;
        return true;
    }

    // Unlock. The mutex must be locked
    void unlock() noexcept
    {
        // Unlock
        assert(locked_);
        locked_ = false;

        // Notify any waiting coroutines
        chan_.try_send(error_code());
    }

    using guard = std::unique_ptr<async_mutex, guard_deleter>;
    guard lock_with_guard(boost::asio::yield_context yield)
    {
        lock(yield);
        return guard(this);
    }
};

}  // namespace chat

#endif
