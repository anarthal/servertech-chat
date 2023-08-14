//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "websocket_session.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/async/with.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/variant2/variant.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>

#include "error.hpp"
#include "redis_client.hpp"
#include "serialization.hpp"
#include "shared_state.hpp"
#include "websocket.hpp"

using namespace chat;

static void fail(chat::error_code ec, char const* what)
{
    // Don't report these
    if (ec != boost::beast::websocket::error::closed)
        chat::log_error(ec, what);
}

chat::websocket_session::websocket_session(websocket socket, std::shared_ptr<shared_state> state)
    : ws_(std::move(socket)), state_(std::move(state))
{
}

// TODO: stub for now
static std::vector<chat::room> get_rooms()
{
    return {
        {"beast", "Boost.Beast"        },
        {"async", "Boost.Async"        },
        {"db",    "Database connectors"},
        {"wasm",  "Web assembly"       },
    };
}

namespace {

// Wrapper coroutine to guarantee lifetimes
static promise<void> write_to_session(
    std::shared_ptr<websocket_session> sess,
    std::shared_ptr<std::string> msg
)
{
    co_await sess->get_websocket().write(*msg);
}

struct event_handler_visitor
{
    chat::websocket& ws;
    chat::shared_state& st;

    // Parsing error
    promise<error_code> operator()(chat::error_code ec) const noexcept { co_return ec; }

    // Messages event
    promise<error_code> operator()(chat::messages_event& evt) const
    {
        // Set the timestamp
        auto timestamp = chat::timestamp_t::clock::now();
        for (auto& msg : evt.messages)
            msg.timestamp = timestamp;

        // Store it in Redis
        auto message_ids_result = co_await st.redis().store_messages(evt.room_id, evt.messages);
        if (message_ids_result.has_error())
            co_return message_ids_result.error();
        auto& message_ids = message_ids_result.value();

        // Set the message IDs appropriately
        assert(message_ids.size() == evt.messages.size());
        for (std::size_t i = 0; i < message_ids.size(); ++i)
            evt.messages[i].id = std::move(message_ids[i]);

        // Broadcast the event to the other clients
        auto stringified_evt = std::make_shared<std::string>(chat::serialize_messages_event(evt));
        auto [first, last] = st.sessions().get_sessions(evt.room_id);
        for (auto it = first; it != last; ++it)
        {
            +write_to_session(it->session, stringified_evt);
        }

        co_return chat::error_code();
    }

    // Request room history event
    promise<error_code> operator()(const chat::request_room_history_event& evt) const
    {
        // Get room history from Redis
        auto history_result = co_await st.redis().get_room_history(evt.room_id, evt.first_message_id);
        if (history_result.has_error())
            co_return history_result.error();
        auto& history = history_result.value();

        // Compose a room_history event
        bool has_more = history.size() >= chat::redis_client::message_batch_size;
        chat::room_history_event response_evt{evt.room_id, std::move(history), has_more};

        // Serialize it
        auto payload = chat::serialize_room_history_event(response_evt);

        // Send it
        auto ec = co_await ws.write(payload);
        co_return ec;
    }
};

struct session_map_deleter
{
    chat::session_map* sessmap{};

    void operator()(chat::websocket_session* sess) const noexcept
    {
        if (sessmap)
            sessmap->remove_session(*sess);
    }
};

using session_map_guard = std::unique_ptr<chat::websocket_session, session_map_deleter>;

}  // namespace

promise<void> chat::websocket_session::run()
{
    // Lock writes in the websocket. This ensures that no message is written before the hello.
    auto guard = ws_.lock_writes();

    // Get the rooms the user is in
    auto rooms = get_rooms();

    // Add the session to the map
    state_->sessions().add_session(shared_from_this(), rooms);
    session_map_guard map_guard{this, session_map_deleter{&state_->sessions()}};

    // Retrieve room history
    auto history = co_await state_->redis().get_room_history(rooms);
    if (history.has_error())
        co_return fail(history.error(), "Retrieving chat history");

    // Send the hello event
    for (std::size_t i = 0; i < rooms.size(); ++i)
        rooms[i].messages = std::move(history.value()[i]);
    auto hello = serialize_hello_event(hello_event{std::move(rooms)});
    error_code ec = co_await ws_.write_locked(hello, guard);
    if (ec)
        co_return fail(ec, "Sending hello event");

    // Unlock
    guard.reset();

    // Read subsequent messages from the websocket and dispatch them
    while (true)
    {
        // Read a message
        auto raw_msg = co_await ws_.read();
        if (raw_msg.has_error())
            co_return fail(raw_msg.error(), "websocket read");

        // Deserialize it
        auto msg = chat::parse_client_event(raw_msg.value());

        // Dispatch
        auto ec = co_await boost::variant2::visit(event_handler_visitor{ws_, *state_}, msg);
        if (ec)
            co_return fail(ec, "processing websocket message");
    }
}