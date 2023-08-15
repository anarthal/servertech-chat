//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "websocket_session.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/async/channel.hpp>
#include <boost/async/generator.hpp>
#include <boost/async/select.hpp>
#include <boost/async/with.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/system/system_error.hpp>
#include <boost/variant2/variant.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
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
    co_await sess->write(std::move(msg));
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

// RAII-style guard to make sure the channel is closed on exit
struct channel_close_deleter
{
    void operator()(boost::async::channel<std::shared_ptr<std::string>>* chan) const noexcept
    {
        chan->close();
    }
};

using channel_close_guard = std::
    unique_ptr<boost::async::channel<std::shared_ptr<std::string>>, channel_close_deleter>;

// Generator to read from the websocket using async::select.
// Using select(ws.read(), channel.read()) will cancel the websocket read if the channel
// fires, which will close the websocket. On the other hand, generators implement
// interrupt_await, which makes it possible to resume them without cancelling them
static boost::async::generator<result<std::string_view>> websocket_generator(websocket& ws)
{
    while (true)
        co_yield co_await ws.read();
}

}  // namespace

promise<void> chat::websocket_session::run()
{
    // Close the write channel on exit. This prevents deadlocks - if another session attempts
    // a write but we're not listening anymore because we're tearing the session down, it won't hang
    channel_close_guard close_guard{&write_channel_};

    // Get the rooms the user is in
    auto rooms = get_rooms();

    // Add the session to the map
    auto map_guard = state_->sessions().add_session_guarded(shared_from_this(), rooms);

    // Retrieve room history
    auto history = co_await state_->redis().get_room_history(rooms);
    if (history.has_error())
        co_return fail(history.error(), "Retrieving chat history");

    // Send the hello event
    for (std::size_t i = 0; i < rooms.size(); ++i)
        rooms[i].messages = std::move(history.value()[i]);
    auto hello = serialize_hello_event(hello_event{std::move(rooms)});
    error_code ec = co_await ws_.write(hello);
    if (ec)
        co_return fail(ec, "Sending hello event");

    // Read subsequent messages from the websocket and dispatch them
    auto ws_gen = websocket_generator(ws_);

    while (true)
    {
        // Either read a message from the client, or from the write channel.
        // If the websocket read completes, the channel read will get cancelled.
        // This result in an internal exception swallowed by select. Issuing
        // another channel read after that is safe.
        auto select_result = co_await boost::async::select(ws_gen, write_channel_.read());

        if (auto* read_result = boost::variant2::get_if<result<std::string_view>>(&select_result))
        {
            // The web client sent us a message over the websocket
            if (read_result->has_error())
                co_return fail(read_result->error(), "websocket read");

            // Deserialize it
            auto msg = chat::parse_client_event(read_result->value());

            // Dispatch
            auto ec = co_await boost::variant2::visit(event_handler_visitor{ws_, *state_}, msg);
            if (ec)
                co_return fail(ec, "processing websocket message");
        }
        else
        {
            // A session sent us a message that should be written back to the web client
            auto msg_to_write = boost::variant2::get<std::shared_ptr<std::string>>(select_result);

            // Write it through the websocket
            ec = co_await ws_.write(*msg_to_write);
            if (ec)
                co_return fail(ec, "writing a message through the websocket");
        }
    }
}

promise<void> chat::websocket_session::write(std::shared_ptr<std::string> msg)
{
    co_await write_channel_.write(std::move(msg));
}
