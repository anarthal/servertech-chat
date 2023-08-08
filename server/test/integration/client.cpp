//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "client.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>
#include <boost/system/result.hpp>
#include <boost/system/system_error.hpp>

#include <memory>
#include <string_view>

using namespace chat::test;
namespace websocket = boost::beast::websocket;
namespace http = boost::beast::http;

static constexpr const char* localhost = "127.0.0.1";
static constexpr unsigned short port = 9876;

static boost::asio::io_context& get_context()
{
    static boost::asio::io_context res(1);
    return res;
}

static void setup_db(boost::redis::connection& conn)
{
    boost::redis::request req;
    boost::redis::generic_response resp;

    // Use a database other than the default one. This makes development easier
    req.push("SELECT", "1");

    // Wipe out anything in the database
    req.push("FLUSHDB");

    // Run these commands
    conn.async_exec(req, resp, boost::asio::use_future).get();
    resp.value();  // throws on error
}

struct websocket_client::impl
{
    websocket::stream<boost::asio::ip::tcp::socket> ws{get_context().get_executor()};
    boost::beast::flat_buffer read_buffer;
};

websocket_client::websocket_client(impl* i) noexcept : impl_(i) {}

websocket_client::websocket_client(websocket_client&& rhs) noexcept : impl_(std::move(rhs.impl_)) {}

websocket_client& websocket_client::operator=(websocket_client&& rhs) noexcept
{
    impl_ = std::move(rhs.impl_);
    return *this;
}
websocket_client::~websocket_client()
{
    try
    {
        error_code ec;
        impl_->ws.close(websocket::close_code::normal, ec);
    }
    catch (...)
    {
    }
}

std::string_view websocket_client::read()
{
    impl_->ws.read(impl_->read_buffer);
    return std::string_view(
        static_cast<const char*>(impl_->read_buffer.data().data()),
        impl_->read_buffer.data().size()
    );
}

void websocket_client::write(std::string_view buffer) { impl_->ws.write(boost::asio::buffer(buffer)); }

server_runner_base::server_runner_base() : app(application_config{"", localhost, port})
{
    // Setup the application
    auto ec = app.setup();
    if (ec)
        throw boost::system::system_error(ec);

    // Start running
    runner = std::thread([this] { this->app.run_until_completion(true); });
}

server_runner_base::~server_runner_base()
{
    app.stop();
    if (runner.joinable())
        runner.join();
}

server_runner::server_runner() { setup_db(app.redis_connection()); }

websocket_client server_runner_base::connect_websocket()
{
    auto impl = std::make_unique<websocket_client::impl>();

    // Make the connection on the IP address we get from a lookup
    impl->ws.next_layer().connect(
        boost::asio::ip::tcp::endpoint{boost::asio::ip::address_v4::loopback(), port}
    );

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    std::string host = "127.0.0.1:" + std::to_string(port);

    // Set a decorator to change the User-Agent of the handshake
    impl->ws.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
        req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
    }));

    // Perform the websocket handshake
    impl->ws.handshake(host, "/");

    return websocket_client(impl.release());
}