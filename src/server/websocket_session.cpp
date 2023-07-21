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
#include <iostream>
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
    if (ec == net::error::operation_aborted || ec == websocket::error::closed)
        return;

    std::cerr << what << ": " << ec << ": " << ec.message() << "\n";
}

chat::websocket_session::websocket_session(net::ip::tcp::socket&& socket, std::shared_ptr<shared_state> state)
    : ws_(std::move(socket)), state_(std::move(state))
{
}

// TODO: this is for testing
static std::vector<std::string> get_rooms() { return {"room1", "room2", "room3"}; }

static void send_rooms(
    chat::ws_stream& ws,
    const std::vector<std::string>& rooms,
    boost::asio::yield_context yield
)
{
    // Serialize them
    auto payload = chat::serialize_rooms_event(rooms);

    // Send them
    ws.async_write(boost::asio::buffer(payload), yield);
}

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
    req.push("XADD", room_name, "*", "msg", msg.content);
    st.redis().async_exec(req, boost::redis::ignore, yield[ec]);
    if (ec)
        return ec;

    // Broadcast it to other clients
    auto [first, last] = st.sessions().get_sessions(room_name);
    std::vector<std::shared_ptr<chat::websocket_session>> sessions;  // Guarantee lifetimes
    for (auto it = first; it != last; ++it)
        sessions.push_back(it->session);
    for (const auto& sess : sessions)
    {
        // TODO: paralelize this
        // TODO: we need to guarantee that writes don't clash
        // TODO: errors are ignored
        sess->stream().async_write(boost::asio::buffer(msg.content), yield[ec]);
    }

    return ec;
}

static void send_chat_history(
    chat::ws_stream& ws,
    const std::vector<chat::message>& msgs,
    boost::asio::yield_context yield
)
{
    auto payload = chat::serialize_messages_event(msgs);
    ws.async_write(boost::asio::buffer(payload), yield);
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

    // Send the rooms the user is in
    send_rooms(ws_, rooms, yield[ec]);
    if (ec)
        return fail(ec, "Sending rooms");

    // Send chat history
    send_chat_history(ws_, history.value(), yield[ec]);
    if (ec)
        return fail(ec, "Sending chat history");

    // Add the session to the map
    state_->sessions().add_session(shared_from_this(), rooms);

    // Read subsequent messages from the websocket and dispatch them
    boost::beast::flat_buffer buff;
    while (true)
    {
        // Read a message
        ws_.async_read(buff, yield[ec]);
        if (ec)
            return fail(ec, "websocket read");

        // Deserialize it
        auto msg = chat::parse_websocket_request(buffer_to_sv(buff.data()));
        const auto* err = boost::variant2::get_if<error_code>(&msg);
        if (err)
            return fail(*err, "bad websocket message");

        // Message dispatch
        ec = process_message(*state_, boost::variant2::get<message>(msg), yield);
        if (ec)
            return fail(ec, "Broadcasting message");

        // Clear the buffer
        buff.consume(buff.size());
    }
}