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
#include <boost/beast/websocket/error.hpp>
#include <boost/variant2/variant.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>

#include "api_serialization.hpp"
#include "error.hpp"
#include "redis_client.hpp"
#include "session_map.hpp"
#include "shared_state.hpp"
#include "websocket.hpp"

using namespace chat;

namespace {

static void fail(chat::error_code ec, char const* what)
{
    // Don't report these
    if (ec != boost::beast::websocket::error::closed)
        chat::log_error(ec, what);
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

struct event_handler_visitor
{
    chat::websocket& ws;
    chat::shared_state& st;
    boost::asio::yield_context yield;

    // Parsing error
    chat::error_code operator()(chat::error_code ec) const noexcept { return ec; }

    // Messages event
    chat::error_code operator()(chat::messages_event& evt) const
    {
        // Set the timestamp
        auto timestamp = chat::timestamp_t::clock::now();
        for (auto& msg : evt.messages)
            msg.timestamp = timestamp;

        // Store it in Redis
        auto message_ids_result = st.redis().store_messages(evt.room_id, evt.messages, yield);
        if (message_ids_result.has_error())
            return message_ids_result.error();
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
            boost::asio::spawn(
                yield.get_executor(),
                [sess = it->session, stringified_evt](boost::asio::yield_context yield) {
                    chat::error_code ec;
                    sess->write(*stringified_evt, yield[ec]);
                },
                boost::asio::detached
            );
        }

        return chat::error_code();
    }

    // Request room history event
    chat::error_code operator()(const chat::request_room_history_event& evt) const
    {
        // Get room history from Redis
        auto history_result = st.redis().get_room_history(evt.room_id, evt.first_message_id, yield);
        if (history_result.has_error())
            return history_result.error();
        auto& history = history_result.value();

        // Compose a room_history event
        bool has_more = history.size() >= chat::redis_client::message_batch_size;
        chat::room_history_event response_evt{evt.room_id, std::move(history), has_more};

        // Serialize it
        auto payload = chat::serialize_room_history_event(response_evt);

        // Send it
        chat::error_code ec;
        ws.write(payload, yield[ec]);
        return ec;
    }
};

}  // namespace

chat::websocket_session::websocket_session(websocket socket, std::shared_ptr<shared_state> state)
    : ws_(std::move(socket)), state_(std::move(state))
{
}

error_code chat::websocket_session::write(std::string_view msg, boost::asio::yield_context yield)
{
    return ws_.write(msg, yield);
}

void chat::websocket_session::run(boost::asio::yield_context yield)
{
    error_code ec;

    // Lock writes in the websocket. This ensures that no message is written before the hello.
    auto write_guard = ws_.lock_writes(yield);

    // Get the rooms the user is in
    auto rooms = get_rooms();

    // Add the session to the map
    auto sessmap_guard = state_->sessions().add_session_guarded(shared_from_this(), rooms);

    // Retrieve room history
    auto history = state_->redis().get_room_history(rooms, yield);
    if (history.has_error())
        return fail(history.error(), "Retrieving chat history");

    // Send the hello event
    for (std::size_t i = 0; i < rooms.size(); ++i)
        rooms[i].messages = std::move(history.value()[i]);
    auto hello = serialize_hello_event(hello_event{std::move(rooms)});
    ec = ws_.write_locked(hello, write_guard, yield);
    if (ec)
        return fail(ec, "Sending hello event");

    // Once the hello is sent, we can start sending message through the websocket
    write_guard.reset();

    // Read subsequent messages from the websocket and dispatch them
    while (true)
    {
        // Read a message
        auto raw_msg = ws_.read(yield);
        if (raw_msg.has_error())
            return fail(raw_msg.error(), "websocket read");

        // Deserialize it
        auto msg = chat::parse_client_event(raw_msg.value());

        // Dispatch
        ec = boost::variant2::visit(event_handler_visitor{ws_, *state_, yield}, msg);
        if (ec)
            return fail(ec, "processing websocket message");
    }
}