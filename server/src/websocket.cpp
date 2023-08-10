//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "websocket.hpp"

#include <boost/asio/experimental/channel.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/core/ignore_unused.hpp>

#include <deque>
#include <memory>
#include <string_view>

#include "async_mutex.hpp"
#include "error.hpp"
#include "use_nothrow_op.hpp"

using namespace chat;

struct chat::websocket::impl
{
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws;
    boost::beast::flat_buffer read_buffer;
    async_mutex write_mtx_;
    bool reading{false};

    impl(boost::asio::ip::tcp::socket&& sock, boost::beast::flat_buffer&& buff)
        : ws(std::move(sock)), read_buffer(std::move(buff)), write_mtx_(ws.get_executor())
    {
    }

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

chat::websocket::websocket(boost::asio::ip::tcp::socket sock, boost::beast::flat_buffer buff)
    : impl_(new impl(std::move(sock), std::move(buff)))
{
}

chat::websocket::websocket(websocket&& rhs) noexcept : impl_(std::move(rhs.impl_)) {}

chat::websocket& chat::websocket::operator=(websocket&& rhs) noexcept
{
    impl_ = std::move(rhs.impl_);
    return *this;
}

chat::websocket::~websocket() {}

promise<error_code> chat::websocket::accept(
    boost::beast::http::request<boost::beast::http::string_body> request
)
{
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
    auto [ec] = co_await impl_->ws.async_accept(request, use_nothrow_op);

    co_return ec;
}

promise<result<std::string_view>> chat::websocket::read()
{
    assert(!impl_->reading);

    {
        auto guard = impl_->lock_reads();
        impl_->read_buffer.clear();
        auto [ec, size] = co_await impl_->ws.async_read(impl_->read_buffer, use_nothrow_op);
        if (ec)
            co_return ec;
        boost::ignore_unused(size);
    }

    auto res = buffer_to_sv(impl_->read_buffer.data());
    std::cout << "(READ) " << res << std::endl;
    co_return res;
}

promise<error_code> chat::websocket::write_locked_impl(std::string_view buff)
{
    assert(impl_->write_mtx_.locked());

    error_code ec;

    co_await impl_->ws.async_write(boost::asio::buffer(buff), use_nothrow_op);
    std::cout << "(WRITE) " << buff << std::endl;

    co_return ec;
}

promise<error_code> chat::websocket::write(std::string_view message)
{
    // Wait for the connection to become iddle
    auto guard = co_await impl_->write_mtx_.lock_with_guard();

    // Write
    co_return co_await write_locked_impl(message);
}

void chat::websocket::lock_writes_impl() noexcept
{
    [[maybe_unused]] bool ok = impl_->write_mtx_.try_lock();
    assert(ok);
}

void chat::websocket::unlock_writes_impl() noexcept { impl_->write_mtx_.unlock(); }