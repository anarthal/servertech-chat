//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "websocket_session.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
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

chat::websocket_session::websocket_session(websocket socket, std::shared_ptr<shared_state> state)
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
    // TODO: get room name
    const char* room_name = "room1";

    // Store it in Redis
    auto ec = st.redis().add_message(room_name, msg, yield);
    if (ec)
        return ec;

    // Compose a messages event
    std::vector<chat::message> vec{msg};  // TODO: shouldn't need this copy
    auto event = std::make_shared<std::string>(chat::serialize_messages_event(vec));

    // Broadcast it to other clients
    auto [first, last] = st.sessions().get_sessions(room_name);
    for (auto it = first; it != last; ++it)
    {
        boost::asio::spawn(
            st.get_executor(),
            [sess = it->session, event](boost::asio::yield_context yield) {
                chat::error_code ec;
                sess->get_websocket().write(event, yield[ec]);
            },
            boost::asio::detached
        );
    }

    return chat::error_code();
}

void chat::websocket_session::run(boost::asio::yield_context yield)
{
    error_code ec;

    // Get the rooms the user is in - stub for now
    auto rooms = get_rooms();

    // Set the current room
    current_room_ = rooms.at(0);

    // Retrieve history for the current room
    auto history = state_->redis().get_room_history(current_room_, yield);
    if (history.has_error())
        return fail(history.error(), "Retrieving chat history");

    // Send the hello event
    auto hello = serialize_hello_event(rooms, history.value());
    ec = ws_.unguarded_write(hello, yield);
    if (ec)
        return fail(ec, "Sending hello event");

    // Add the session to the map
    state_->sessions().add_session(shared_from_this(), rooms);

    // Read subsequent messages from the websocket and dispatch them
    while (true)
    {
        // Read a message
        auto raw_msg = ws_.read(yield);
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
    }
}