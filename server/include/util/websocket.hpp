//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_WEBSOCKET_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_WEBSOCKET_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <memory>
#include <string_view>

#include "error.hpp"

namespace chat {

// A wrapper around beast's websocket stream that handles concurrent writes
// and reduces build times by keeping Beast instantiations in a separate .cpp file.
class websocket
{
    // pimpl idiom, to avoid including heavyweight Beast headers
    struct impl;
    std::unique_ptr<impl> impl_;

    error_code write_locked_impl(std::string_view buff, boost::asio::yield_context yield);
    void lock_writes_impl(boost::asio::yield_context yield) noexcept;
    void unlock_writes_impl() noexcept;

    struct write_guard_deleter
    {
        void operator()(websocket* sock) const noexcept { sock->unlock_writes_impl(); }
    };

public:
    using upgrade_request_type = boost::beast::http::request<boost::beast::http::string_body>;

    // Constructors, assignments, destructor
    websocket(
        boost::asio::ip::tcp::socket sock,
        upgrade_request_type&& upgrade_request,
        boost::beast::flat_buffer buffer
    );
    websocket(const websocket&) = delete;
    websocket(websocket&&) noexcept;
    websocket& operator=(const websocket&) = delete;
    websocket& operator=(websocket&&) noexcept;
    ~websocket();

    // Returns the upgrade HTTP request
    const upgrade_request_type& upgrade_request() const noexcept;

    // Runs the websocket handshake. Must be called before any other operation
    error_code accept(boost::asio::yield_context yield);

    // Reads a message from the client. The returned view is valid until the next
    // read is performed. Only a single read should be outstanding at each time
    // (unlike writes, reads are not serialized).
    result<std::string_view> read(boost::asio::yield_context yield);

    // Writes a message to the client. Writes are serialized: two
    // concurrent writes can be issued safely against the same websocket.
    // A write is roughly equivalent to lock_writes() + write_locked() + releasing the guard
    error_code write(std::string_view buff, boost::asio::yield_context yield);

    // Locks writes until the returned guard is destroyed. Other coroutines
    // calling write will be suspended until the guard is released.
    using write_guard = std::unique_ptr<websocket, write_guard_deleter>;
    write_guard lock_writes(boost::asio::yield_context yield)
    {
        lock_writes_impl(yield);
        return write_guard(this);
    }

    // Writes bypassing the write lock. lock_writes() must have been called
    // before calling this function.
    error_code write_locked(
        std::string_view buff,
        [[maybe_unused]] write_guard& guard,
        boost::asio::yield_context yield
    )
    {
        assert(guard.get() != nullptr);
        return write_locked_impl(buff, yield);
    }

    // Closes the websocket, sending close_code to the client.
    error_code close(unsigned close_code, boost::asio::yield_context yield);
};

}  // namespace chat

#endif
