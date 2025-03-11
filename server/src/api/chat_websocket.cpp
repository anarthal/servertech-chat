//
// Copyright (c) 2023-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "api/chat_websocket.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/variant2/variant.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "api/api_types.hpp"
#include "business_types.hpp"
#include "error.hpp"
#include "services/cookie_auth_service.hpp"
#include "services/pubsub_service.hpp"
#include "services/redis_client.hpp"
#include "services/room_history_service.hpp"
#include "shared_state.hpp"
#include "util/websocket.hpp"

using namespace chat;
namespace asio = boost::asio;

namespace {

// Rooms are static for now.
static constexpr std::array<std::string_view, 4> room_ids{
    "beast",
    "async",
    "db",
    "wasm",
};

static constexpr std::array<std::string_view, room_ids.size()> room_names{
    "Boost.Beast",
    "Boost.Async",
    "Database connectors",
    "Web assembly",
};

// An owning type containing data for the hello event.
struct hello_data
{
    std::vector<room> rooms;
    username_map usernames;
};

// Retrieves the data required to send the hello event
static asio::awaitable<result_with_message<hello_data>> get_hello_data(shared_state& st)
{
    // Retrieve room history
    room_history_service history_service(st.redis(), st.mysql());
    auto history_result = co_await history_service.get_room_history(room_ids);
    if (history_result.has_error())
        co_return std::move(history_result).error();
    assert(history_result->first.size() == room_ids.size());

    // Compose hello data
    hello_data res{{}, std::move(history_result->second)};
    res.rooms.reserve(room_ids.size());
    for (std::size_t i = 0; i < room_ids.size(); ++i)
    {
        res.rooms.push_back(
            room{std::string(room_ids[i]), std::string(room_names[i]), std::move(history_result->first[i])}
        );
    }

    co_return res;
}

struct event_handler_visitor
{
    const user& current_user;
    websocket& ws;
    shared_state& st;

    // Parsing error
    asio::awaitable<error_with_message> operator()(error_code ec) const noexcept
    {
        co_return error_with_message{ec};
    }

    // Messages event
    asio::awaitable<error_with_message> operator()(client_messages_event& evt) const
    {
        // Set the timestamp
        auto timestamp = timestamp_t::clock::now();

        // Compose a message array
        std::vector<message> msgs;
        msgs.reserve(evt.messages.size());
        for (auto& msg : evt.messages)
        {
            msgs.push_back(message{
                "",  // blank ID, will be assigned by Redis
                std::move(msg.content),
                timestamp,
                current_user.id,
            });
        }

        // Store it in Redis
        auto ids_result = co_await st.redis().store_messages(evt.roomId, msgs);
        if (ids_result.has_error())
            co_return std::move(ids_result).error();
        auto& ids = ids_result.value();

        // Set the message IDs appropriately
        assert(msgs.size() == ids.size());
        for (std::size_t i = 0; i < msgs.size(); ++i)
            msgs[i].id = std::move(ids[i]);

        // Compose a server_messages event with all data we have
        server_messages_event server_evt{evt.roomId, current_user, msgs};

        // Broadcast the event to all clients
        st.pubsub().publish(evt.roomId, server_evt.to_json());
        co_return error_with_message{};
    }

    // Request room history event
    asio::awaitable<error_with_message> operator()(chat::request_room_history_event& evt) const
    {
        // Get room history
        room_history_service svc(st.redis(), st.mysql());
        auto history = co_await svc.get_room_history(evt.roomId);
        if (history.has_error())
            co_return std::move(history).error();

        // Compose a room_history event
        chat::room_history_event response_evt{evt.roomId, history->first, history->second};
        auto payload = response_evt.to_json();

        // Send it
        co_return co_await ws.write(payload);
    }
};

// Messages are broadcast between sessions using the pubsub_service.
// We must implement the message_subscriber interface to use it.
// Each websocket session becomes a subscriber.
// We use room IDs as topic IDs, and websocket message payloads as subscription messages.
class chat_websocket_session final : public message_subscriber,
                                     public std::enable_shared_from_this<chat_websocket_session>
{
    websocket ws_;
    std::shared_ptr<shared_state> st_;

public:
    chat_websocket_session(websocket socket, std::shared_ptr<shared_state> state) noexcept
        : ws_(std::move(socket)), st_(std::move(state))
    {
    }

    // Subscriber callback
    asio::awaitable<void> on_message(std::string_view serialized_message) override final
    {
        co_await ws_.write(serialized_message);  // Ignore error code (TODO: log it?)
    }

    // Runs the session until completion
    asio::awaitable<error_with_message> run()
    {
        error_code ec;

        // Check that the user is authenticated
        auto user_result = co_await st_->cookie_auth().user_from_cookie(ws_.upgrade_request());
        if (user_result.has_error())
        {
            // If it's not, close the websocket. This is the preferred approach
            // when checking authentication in websockets, as opposed to failing
            // the websocket upgrade, since the client doesn't have access to
            // upgrade failure information.
            log_error(user_result.error(), "Websocket authentication failed");
            co_await ws_.close(boost::beast::websocket::policy_error);  // Ignore the result
            co_return error_with_message{};
        }
        const auto& current_user = user_result.value();

        // Lock writes in the websocket. This ensures that no message is written before the hello.
        auto write_guard = co_await ws_.lock_writes();

        // Subscribe to messages for the available rooms
        auto pubsub_guard = st_->pubsub().subscribe_guarded(shared_from_this(), room_ids);

        // Retrieve the data required for the hello message
        auto hello_data = co_await get_hello_data(*st_);
        if (hello_data.has_error())
            co_return hello_data.error();

        // Compose the hello event and write it
        hello_event hello_evt{current_user, hello_data->rooms, hello_data->usernames};
        auto serialized_hello = hello_evt.to_json();
        ec = co_await ws_.write_locked(serialized_hello, write_guard);
        if (ec)
            co_return error_with_message{ec};

        // Once the hello is sent, we can start sending messages through the websocket
        write_guard.reset();

        // Read subsequent messages from the websocket and dispatch them
        while (true)
        {
            // Read a message
            auto raw_msg = co_await ws_.read();
            if (raw_msg.has_error())
                co_return error_with_message{raw_msg.error()};

            // Deserialize it
            auto msg = chat::parse_client_event(raw_msg.value());

            // Dispatch
            auto err = co_await boost::variant2::visit(event_handler_visitor{current_user, ws_, *st_}, msg);
            if (err.ec)
                co_return err;
        }
    }
};

}  // namespace

asio::awaitable<error_with_message> chat::handle_chat_websocket(
    websocket socket,
    std::shared_ptr<shared_state> state
)
{
    auto sess = std::make_shared<chat_websocket_session>(std::move(socket), std::move(state));
    co_return co_await sess->run();
}