//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "websocket_session.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>
#include <boost/variant2/variant.hpp>

#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "error.hpp"
#include "serialization.hpp"
#include "shared_state.hpp"

namespace net = boost::asio;
namespace websocket = boost::beast::websocket;

static std::string_view buffer_to_sv(boost::asio::const_buffer buff) noexcept
{
    return std::string_view(static_cast<const char*>(buff.data()), buff.size());
}

static void fail(chat::error_code ec, char const* what)
{
    // Don't report these
    if (ec != websocket::error::closed)
        chat::log_error(ec, what);
}

chat::websocket_session::websocket_session(net::ip::tcp::socket&& socket, std::shared_ptr<shared_state> state)
    : ws_(std::move(socket)), state_(std::move(state))
{
}

// TODO: this is for testing
static std::vector<std::string> get_rooms() { return {"room1", "room2", "room3"}; }

static chat::error_code process_message(
    chat::shared_state& st,
    const chat::message& msg,
    boost::asio::yield_context yield
)
{
    chat::error_code ec;

    // TODO: get room name
    const char* room_name = "room1";

    // Store it in Redis
    boost::redis::request req;
    req.push("XADD", room_name, "*", "payload", chat::serialize_redis_message(msg));
    st.redis().async_exec(req, boost::redis::ignore, yield[ec]);
    if (ec)
        return ec;

    // Compose a messages event
    std::vector<chat::message> vec{msg};
    auto event = chat::serialize_messages_event(vec);  // TODO: shouldn't need this copy

    // Broadcast it to other clients
    auto [first, last] = st.sessions().get_sessions(room_name);
    for (auto it = first; it != last; ++it)
    {
        // TODO: optimize these copies
        it->session->queue_write(event);
    }

    return ec;
}

static chat::result<std::vector<chat::message>> get_room_history(
    chat::shared_state& st,
    std::string_view room_name,
    boost::asio::yield_context yield
)
{
    boost::redis::request req;
    boost::redis::generic_response res;
    chat::error_code ec;

    req.push("XREVRANGE", room_name, "+", "-");
    st.redis().async_exec(req, res, yield[ec]);

    if (ec)
        return ec;

    return chat::parse_room_history(res);
}

static void log_write(std::string_view what) { std::cout << "(WRITE) " << what << std::endl; }

chat::result<std::string_view> chat::websocket_session::read(boost::asio::yield_context yield)
{
    assert(!reading_);

    error_code ec;

    reading_ = true;  // TODO: RAII-fy this
    buffer_.clear();
    ws_.async_read(buffer_, yield[ec]);
    reading_ = false;
    if (ec)
        return ec;
    auto res = buffer_to_sv(buffer_.data());
    std::cout << "(READ) " << res << std::endl;
    return res;
}

void chat::websocket_session::trigger_write()
{
    assert(currently_writing_ == currently_writing_t::none);
    if (!buffer_1_.empty())
    {
        currently_writing_ = currently_writing_t::buff_1;
        ws_.async_write(
            boost::asio::buffer(buffer_1_),
            [self = shared_from_this()](error_code ec, std::size_t) {
                log_write(self->buffer_1_);
                self->buffer_1_.clear();
                self->currently_writing_ = currently_writing_t::none;
                self->write_error_ = ec;
                if (!self->buffer_2_.empty())
                    self->trigger_write();
            }
        );
    }
    else
    {
        assert(!buffer_2_.empty());
        currently_writing_ = currently_writing_t::buff_2;
        ws_.async_write(
            boost::asio::buffer(buffer_2_),
            [self = shared_from_this()](error_code ec, std::size_t) {
                log_write(self->buffer_2_);
                self->buffer_2_.clear();
                self->currently_writing_ = currently_writing_t::none;
                self->write_error_ = ec;
                if (!self->buffer_1_.empty())
                    self->trigger_write();
            }
        );
    }
}

void chat::websocket_session::queue_write(std::string_view message)
{
    if (currently_writing_ == currently_writing_t::none)
    {
        // Add to buffer 1
        buffer_1_ = message;

        // Trigger a write
        trigger_write();
    }
    else if (currently_writing_ == currently_writing_t::buff_1)
    {
        // Add to buffer 2 and do nothing
        buffer_2_.append(message.begin(), message.end());
    }
    else
    {
        // Add to buffer 1 and do nothing
        buffer_1_.append(message.begin(), message.end());
    }
}

void chat::websocket_session::run(
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
    if (ec)
        return fail(ec, "accept");

    // Get the rooms the user is in - stub for now
    auto rooms = get_rooms();

    // Set the current room
    current_room_ = rooms.at(0);

    // Retrieve history for the current room
    auto history = get_room_history(*state_, current_room_, yield);
    if (history.has_error())  // TODO: this is wrong
        return fail(history.error(), "Retrieving chat history");

    // Send the hello event
    auto hello = serialize_hello_event(rooms, history.value());
    ws_.async_write(net::buffer(hello), yield[ec]);
    if (ec)
        return fail(ec, "Sending hello event");
    log_write(hello);

    // Add the session to the map
    state_->sessions().add_session(shared_from_this(), rooms);

    // Read subsequent messages from the websocket and dispatch them
    while (true)
    {
        // Read a message
        auto raw_msg = read(yield);
        if (raw_msg.has_error())
            return fail(raw_msg.error(), "websocket read");

        // Deserialize it
        auto msg = chat::parse_websocket_request(raw_msg.value());
        const auto* err = boost::variant2::get_if<error_code>(&msg);
        if (err)
            return fail(*err, "bad websocket message");

        // Message dispatch
        ec = process_message(*state_, boost::variant2::get<message>(msg), yield);
        if (ec)
            return fail(ec, "Broadcasting message");

        // If a write operation failed, report it and close the connection
        if (write_error_)
            return fail(write_error_, "Write failed");
    }
}