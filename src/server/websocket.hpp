// copyright
#ifndef BOOST_MYSQL_
#define BOOST_MYSQL_

#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <deque>
#include <memory>
#include <string>
#include <string_view>

#include "error.hpp"

namespace chat {

using ws_stream = boost::beast::websocket::stream<boost::beast::tcp_stream>;

// A wrapper around beast's websocket stream that handles concurrent writes.
// Having this in a separate .cpp makes re-builds less painful
class websocket
{
    ws_stream ws_;
    boost::beast::flat_buffer read_buffer_;
    std::deque<boost::asio::experimental::channel<void(error_code)>*> pending_requests;

    bool reading_{false}, writing_{false};

public:
    websocket(boost::asio::ip::tcp::socket sock);

    error_code accept(
        boost::beast::http::request<boost::beast::http::string_body>,
        boost::asio::yield_context yield
    );
    result<std::string_view> read(boost::asio::yield_context yield);
    error_code unguarded_write(std::string_view buff, boost::asio::yield_context yield);
    error_code write(std::shared_ptr<std::string> message, boost::asio::yield_context yield);
};

}  // namespace chat

#endif
