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

#include <deque>
#include <memory>
#include <string_view>

#include "error.hpp"

struct chat::websocket::impl
{
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws;
    boost::beast::flat_buffer read_buffer;
    std::deque<boost::asio::experimental::channel<void(error_code)>*> pending_requests;
    bool reading{false};
    bool writing{false};

    impl(boost::asio::ip::tcp::socket&& sock, boost::beast::flat_buffer&& buff)
        : ws(std::move(sock)), read_buffer(std::move(buff))
    {
    }

    struct read_guard_deleter
    {
        void operator()(impl* self) const noexcept { self->reading = false; }
    };
    using read_guard = std::unique_ptr<impl, read_guard_deleter>;
    read_guard mark_as_reading() noexcept
    {
        reading = true;
        return read_guard(this);
    }

    struct write_guard_deleter
    {
        void operator()(impl* self) const noexcept { self->writing = false; }
    };
    using write_guard = std::unique_ptr<impl, write_guard_deleter>;
    write_guard mark_as_writing() noexcept
    {
        writing = true;
        return write_guard(this);
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

chat::error_code chat::websocket::accept(
    boost::beast::http::request<boost::beast::http::string_body> request,
    boost::asio::yield_context yield
)
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
    impl_->ws.async_accept(request, yield[ec]);

    return ec;
}

chat::result<std::string_view> chat::websocket::read(boost::asio::yield_context yield)
{
    assert(!impl_->reading);

    error_code ec;

    {
        auto guard = impl_->mark_as_reading();
        impl_->read_buffer.clear();
        impl_->ws.async_read(impl_->read_buffer, yield[ec]);
    }

    if (ec)
        return ec;
    auto res = buffer_to_sv(impl_->read_buffer.data());
    std::cout << "(READ) " << res << std::endl;
    return res;
}

chat::error_code chat::websocket::unguarded_write(std::string_view buff, boost::asio::yield_context yield)
{
    assert(!impl_->writing);

    error_code ec;

    {
        auto guard = impl_->mark_as_writing();
        impl_->ws.async_write(boost::asio::buffer(buff), yield[ec]);
        std::cout << "(WRITE) " << buff << std::endl;
    }

    if (!impl_->pending_requests.empty())
    {
        bool ok = impl_->pending_requests.front()->try_send(error_code());
        assert(ok);
        impl_->pending_requests.pop_front();
    }

    return ec;
}

chat::error_code chat::websocket::write(std::string_view message, boost::asio::yield_context yield)
{
    if (!impl_->writing)
    {
        return unguarded_write(message, yield);
    }
    else
    {
        boost::asio::experimental::channel<void(error_code)> chan{impl_->ws.get_executor(), 1};
        impl_->pending_requests.push_back(&chan);

        error_code ec;
        chan.async_receive(yield[ec]);
        assert(!ec);

        return unguarded_write(message, yield);
    }
}
