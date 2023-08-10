//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_ASYNC_MUTEX_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_ASYNC_MUTEX_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/async/op.hpp>

#include <cassert>
#include <deque>
#include <memory>

#include "error.hpp"
#include "promise.hpp"

namespace chat {

// An asynchronous mutex to guarantee mutual exclusion in async code
class async_mutex
{
    bool locked_{false};
    boost::asio::experimental::channel<void(error_code)> chan_;

    struct guard_deleter
    {
        void operator()(async_mutex* self) const noexcept { self->unlock(); }
    };

public:
    async_mutex(boost::asio::any_io_executor ex) : chan_(std::move(ex)) {}
    async_mutex(const async_mutex&) = delete;
    async_mutex(async_mutex&&) = default;
    async_mutex& operator=(const async_mutex&) = delete;
    async_mutex& operator=(async_mutex&&) = default;
    ~async_mutex() = default;

    // Is the mutex locked?
    bool locked() const noexcept { return locked_; }

    // Suspends the current coroutine until the mutex can be acquired, then acquire it
    promise<void> lock()
    {
        // Most of the time this loop will be executed zero times (if unlocked)
        // or once (if locked). Race conditions could make another coroutine, different
        // from the one waked by async_receive, acquire the lock before it. This loop guards against it.
        while (locked_)
        {
            [[maybe_unused]] auto [ec] = co_await chan_.async_receive(
                boost::asio::as_tuple(boost::async::use_op)
            );
            assert(!ec);
        }
        locked_ = true;
    }

    // Try to acquire without suspending
    bool try_lock() noexcept
    {
        if (!locked_)
            locked_ = true;
        return locked_;
    }

    // Unlock. Needs to be locked
    void unlock() noexcept
    {
        assert(locked_);
        locked_ = false;
        chan_.try_send(error_code());
    }

    // RAII-style lock
    using guard = std::unique_ptr<async_mutex, guard_deleter>;
    promise<guard> lock_with_guard()
    {
        co_await lock();
        co_return guard(this);
    }
};

}  // namespace chat

#endif
