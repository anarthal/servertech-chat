// header

#include "client.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>
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
websocket_client::~websocket_client() {}

std::string_view websocket_client::read()
{
    impl_->ws.read(impl_->read_buffer);
    return std::string_view(
        static_cast<const char*>(impl_->read_buffer.data().data()),
        impl_->read_buffer.data().size()
    );
}

void websocket_client::write(std::string_view buffer) { impl_->ws.write(boost::asio::buffer(buffer)); }

server_runner::server_runner() : app(application_config{"", localhost, port})
{
    auto ec = app.setup();
    if (ec)
        throw boost::system::system_error(ec);
    runner = std::thread([this] { this->app.run_until_completion(true); });
}

server_runner::~server_runner()
{
    app.stop();
    if (runner && runner->joinable())
        runner->join();
}

websocket_client server_runner::connect_websocket()
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