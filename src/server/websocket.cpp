//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "websocket.hpp"

#include <string_view>

#include "error.hpp"

static std::string_view buffer_to_sv(boost::asio::const_buffer buff) noexcept
{
    return std::string_view(static_cast<const char*>(buff.data()), buff.size());
}

chat::websocket::websocket(boost::asio::ip::tcp::socket sock) : ws_(std::move(sock)) {}

chat::error_code chat::websocket::accept(
    boost::beast::http::request<boost::beast::http::string_body> request,
    boost::asio::yield_context yield
)
{
    error_code ec;

    // Set suggested timeout settings for the websocket
    ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws_.set_option(
        boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::response_type& res) {
            res.set(
                boost::beast::http::field::server,
                std::string(BOOST_BEAST_VERSION_STRING) + " websocket-chat-multi"
            );
        })
    );

    // Accept the websocket handshake
    ws_.async_accept(request, yield[ec]);

    return ec;
}

chat::result<std::string_view> chat::websocket::read(boost::asio::yield_context yield)
{
    assert(!reading_);

    error_code ec;

    reading_ = true;  // TODO: RAII-fy this
    read_buffer_.clear();
    ws_.async_read(read_buffer_, yield[ec]);
    reading_ = false;
    if (ec)
        return ec;
    auto res = buffer_to_sv(read_buffer_.data());
    std::cout << "(READ) " << res << std::endl;
    return res;
}

chat::error_code chat::websocket::unguarded_write(std::string_view buff, boost::asio::yield_context yield)
{
    assert(!writing_);

    error_code ec;

    // TODO: RAIIfy
    writing_ = true;
    ws_.async_write(boost::asio::buffer(buff), yield[ec]);
    std::cout << "(WRITE) " << buff << std::endl;
    writing_ = false;

    if (!pending_requests.empty())
    {
        bool ok = pending_requests.front()->try_send(error_code());
        assert(ok);
        pending_requests.pop_front();
    }

    return ec;
}

chat::error_code chat::websocket::write(
    std::shared_ptr<std::string> message,
    boost::asio::yield_context yield
)
{
    if (!writing_)
    {
        return unguarded_write(*message, yield);
    }
    else
    {
        boost::asio::experimental::channel<void(error_code)> chan{ws_.get_executor(), 1};
        pending_requests.push_back(&chan);

        error_code ec;
        chan.async_receive(yield[ec]);
        assert(!ec);

        return unguarded_write(*message, yield);
    }
}
