//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/websocket.hpp"

#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <deque>
#include <memory>
#include <string_view>

#include "error.hpp"
#include "util/async_mutex.hpp"

using namespace chat;

struct websocket::impl
{
    // The actual websocket
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws;

    // The upgrade HTTP request
    websocket::upgrade_request_type upgrade_request;

    // Buffer to read data from the client
    boost::beast::flat_buffer read_buffer;

    // Mutex to serialize writes
    async_mutex write_mtx_;

    // Make sure that we don't issue two reads concurrently
    bool reading{false};

    impl(
        boost::asio::ip::tcp::socket&& sock,
        websocket::upgrade_request_type&& upgrade_req,
        boost::beast::flat_buffer&& buff
    )
        : ws(std::move(sock)),
          upgrade_request(std::move(upgrade_req)),
          read_buffer(std::move(buff)),
          write_mtx_(ws.get_executor())
    {
    }

    // Sets and clears the reading flag using RAII
    struct read_guard_deleter
    {
        void operator()(impl* self) const noexcept { self->reading = false; }
    };
    using read_guard = std::unique_ptr<impl, read_guard_deleter>;
    read_guard lock_reads() noexcept
    {
        reading = true;
        return read_guard(this);
    }
};

static std::string_view buffer_to_sv(boost::asio::const_buffer buff) noexcept
{
    return std::string_view(static_cast<const char*>(buff.data()), buff.size());
}

websocket::websocket(
    boost::asio::ip::tcp::socket sock,
    upgrade_request_type&& req,
    boost::beast::flat_buffer buff
)
    : impl_(new impl(std::move(sock), std::move(req), std::move(buff)))
{
}

websocket::websocket(websocket&& rhs) noexcept : impl_(std::move(rhs.impl_)) {}

websocket& websocket::operator=(websocket&& rhs) noexcept
{
    impl_ = std::move(rhs.impl_);
    return *this;
}

websocket::~websocket() {}

const websocket::upgrade_request_type& websocket::upgrade_request() const noexcept
{
    return impl_->upgrade_request;
}

error_code websocket::accept(boost::asio::yield_context yield)
{
    error_code ec;

    // Set suggested timeout settings for the websocket
    impl_->ws.set_option(
        boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server)
    );

    // Set a decorator to change the Server of the handshake
    impl_->ws.set_option(
        boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::response_type& res) {
            res.set(
                boost::beast::http::field::server,
                std::string(BOOST_BEAST_VERSION_STRING) + " websocket-chat-multi"
            );
        })
    );

    // Accept the websocket handshake
    impl_->ws.async_accept(impl_->upgrade_request, yield[ec]);

    return ec;
}

result<std::string_view> websocket::read(boost::asio::yield_context yield)
{
    assert(!impl_->reading);

    error_code ec;

    // Perform the read
    {
        auto guard = impl_->lock_reads();
        impl_->read_buffer.clear();
        impl_->ws.async_read(impl_->read_buffer, yield[ec]);
    }

    // Check the result
    if (ec)
        return ec;

    // Convert it to a string_view (no copy is performed)
    auto res = buffer_to_sv(impl_->read_buffer.data());

    // Log it
    std::cout << "(READ) " << res << std::endl;

    return res;
}

error_code websocket::write_locked_impl(std::string_view buff, boost::asio::yield_context yield)
{
    assert(impl_->write_mtx_.locked());

    error_code ec;

    // Perform the write
    impl_->ws.async_write(boost::asio::buffer(buff), yield[ec]);

    // Log it
    std::cout << "(WRITE) " << buff << std::endl;

    return ec;
}

error_code websocket::write(std::string_view message, boost::asio::yield_context yield)
{
    // Wait for the connection to become iddle
    auto guard = lock_writes(yield);

    // Write
    return write_locked(message, guard, yield);
}

void websocket::lock_writes_impl(boost::asio::yield_context yield) noexcept { impl_->write_mtx_.lock(yield); }

void websocket::unlock_writes_impl() noexcept { impl_->write_mtx_.unlock(); }

error_code websocket::close(unsigned close_code, boost::asio::yield_context yield)
{
    error_code ec;
    impl_->ws.async_close(boost::beast::websocket::close_reason(close_code), yield[ec]);
    return ec;
}