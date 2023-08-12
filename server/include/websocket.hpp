//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_WEBSOCKET_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_WEBSOCKET_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <memory>
#include <string_view>

#include "error.hpp"
#include "promise.hpp"

namespace chat {

// A wrapper around beast's websocket stream that handles concurrent writes
// and reduces build times by keeping Beast instantiations in a separate .cpp file.
class websocket
{
    struct impl;
    std::unique_ptr<impl> impl_;

    promise<error_code> write_locked_impl(std::string_view buff);
    void lock_writes_impl() noexcept;
    promise<void> unlock_writes_impl() noexcept;

public:
    websocket(boost::asio::ip::tcp::socket sock, boost::beast::flat_buffer buffer);
    websocket(const websocket&) = delete;
    websocket(websocket&&) noexcept;
    websocket& operator=(const websocket&) = delete;
    websocket& operator=(websocket&&) noexcept;
    ~websocket();

    // Accepting connections
    promise<error_code> accept(boost::beast::http::request<boost::beast::http::string_body> upgrade_request);

    // Reading. Only a single concurrent read is allowed. The result is valid until the next read
    promise<result<std::string_view>> read();

    // Writing. Multiple concurrent writes are supported.
    promise<error_code> write(std::string_view buff);

    // Locks writes until the received guard is destroyed. Other coroutines
    // calling write will be suspended until the guard is released.
    struct write_guard
    {
        websocket* self;

        promise<void> await_exit(std::exception_ptr) const
        {
            if (self)
                co_await self->unlock_writes_impl();
        }
    };
    write_guard lock_writes()
    {
        lock_writes_impl();
        return write_guard{this};
    }

    // Writes bypassing the write lock. lock_writes() must have been called before calling this function
    promise<error_code> write_locked(std::string_view buff, [[maybe_unused]] write_guard& guard)
    {
        assert(guard.self != nullptr);
        co_return co_await write_locked_impl(buff);
    }
};

}  // namespace chat

#endif
