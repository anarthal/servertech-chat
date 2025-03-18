//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_WEBSOCKET_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_WEBSOCKET_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/system/error_code.hpp>

#include <memory>
#include <string_view>

namespace chat {

// A wrapper around beast's websocket stream that handles concurrent writes
// and reduces build times by keeping Beast instantiations in a separate .cpp file.
class websocket
{
    // pimpl idiom, to avoid including heavyweight Beast headers
    struct impl;
    std::unique_ptr<impl> impl_;

    boost::asio::awaitable<boost::system::error_code> write_locked_impl(std::string_view buff);
    boost::asio::awaitable<void> lock_writes_impl();
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
    boost::asio::awaitable<boost::system::error_code> accept();

    // Reads a message from the client. The returned view is valid until the next
    // read is performed. Only a single read should be outstanding at each time
    // (unlike writes, reads are not serialized).
    boost::asio::awaitable<boost::system::result<std::string_view>> read();

    // Writes a message to the client. Writes are serialized: two
    // concurrent writes can be issued safely against the same websocket.
    // A write is roughly equivalent to lock_writes() + write_locked() + releasing the guard
    boost::asio::awaitable<boost::system::error_code> write(std::string_view buff);

    // Locks writes until the returned guard is destroyed. Other coroutines
    // calling write will be suspended until the guard is released.
    using write_guard = std::unique_ptr<websocket, write_guard_deleter>;
    boost::asio::awaitable<write_guard> lock_writes()
    {
        co_await lock_writes_impl();
        co_return write_guard(this);
    }

    // Writes bypassing the write lock. lock_writes() must have been called
    // before calling this function.
    boost::asio::awaitable<boost::system::error_code> write_locked(
        std::string_view buff,
        [[maybe_unused]] write_guard& guard
    )
    {
        assert(guard.get() != nullptr);
        return write_locked_impl(buff);
    }

    // Closes the websocket, sending close_code to the client.
    boost::asio::awaitable<boost::system::error_code> close(unsigned close_code);
};

}  // namespace chat

#endif
