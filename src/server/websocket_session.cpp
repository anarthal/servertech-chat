//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "websocket_session.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/cancellation_condition.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/describe/class.hpp>
#include <boost/json/array.hpp>
#include <boost/json/fwd.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/response.hpp>
#include <boost/variant2/variant.hpp>

#include <cstddef>
#include <iostream>
#include <memory>
#include <string_view>
#include <unordered_map>

#include "error.hpp"
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

    std::cerr << what << ": " << ec.message() << "\n";
}

chat::websocket_session::websocket_session(net::ip::tcp::socket&& socket, std::shared_ptr<shared_state> state)
    : ws_(std::move(socket)), state_(std::move(state))
{
}

namespace chat {
struct message
{
    std::string user;
    std::string message;
};
BOOST_DESCRIBE_STRUCT(message, (), (user, message))
}  // namespace chat

// static std::vector<chat::message> parse_stream_response(const boost::redis::generic_response& resp)
// {
//     std::vector<chat::message> res;
//     std::size_t item_index = 0;
//     // TODO: handle this better
//     if (!resp.has_value())
//         return res;

//     const std::vector<boost::redis::resp3::node>& nodes = resp.value();

//     for (std::size_t i = 0; i < nodes.size(); ++i)
//     {
//     }

//     return res;
// }

// TODO: this is for testing
static std::vector<std::string> get_rooms() { return {"room1", "room2", "room3"}; }

// static std::string serialize_messages(const std::vector<chat::message>& input)
// {
//     return boost::json::serialize(boost::json::value_from(input));
// }

// static constexpr const char* group_name = "myrgoup";

// static void message_consumer(
//     boost::redis::connection& conn,
//     chat::ws_stream& ws,
//     boost::asio::yield_context yield
// )
// {
//     // # array<StreamMsgs>
//     // # StreamMsgs: tuple<StreamId /*string*/, array<Msg>>
//     // # Msg: tuple<MsgId /*string*/, map<string, string>>

//     std::string last_id = "$";
//     chat::error_code ec;
//     boost::redis::generic_response resp;

//     // Loop while reconnection is enabled
//     while (conn.will_reconnect())
//     {
//         // Wait for messages to be received
//         boost::redis::request req;
//         req.push("XREAD", "BLOCK", "0", "STREAMS", group_name, last_id);
//         conn.async_exec(req, resp, yield[ec]);
//         if (ec)
//             break;  // connection lost?

//         // Parse the messages
//         auto msgs = parse_stream_response(resp);

//         // Serialize them into a JSON object
//         auto json = serialize_messages(msgs);

//         // Send them through the websocket
//         ws.async_write(boost::asio::buffer(json), yield[ec]);
//         if (ec)
//             break;
//     }
// }

// static void message_producer(
//     boost::redis::connection& conn,
//     chat::ws_stream& ws_,
//     boost::asio::yield_context yield
// )
// {
//     boost::beast::flat_buffer buff;
//     chat::error_code ec;

//     while (true)
//     {
//         // Read a message
//         ws_.async_read(buff, yield[ec]);
//         if (ec)
//             return fail(ec, "websocket read");

//         // Publish it to Redis
//         boost::redis::request redis_req;
//         redis_req.push("XADD", group_name, "*", "user", "some_user", "msg", buffer_to_sv(buff.data()));
//         conn.async_exec(redis_req, boost::redis::ignore, yield[ec]);
//         if (ec)
//             return fail(ec, "Redis publish");  // TODO: this is not robust

//         // Clear the buffer
//         buff.consume(buff.size());
//     }
// }

static void send_rooms(
    chat::ws_stream& ws,
    const std::vector<std::string>& rooms,
    boost::asio::yield_context yield
)
{
    // Serialize them
    boost::json::value res{
        {"type",  "rooms"             },
        {"rooms", boost::json::array()}
    };
    auto& json_rooms = res.at("rooms").as_array();
    json_rooms.reserve(rooms.size());
    for (const auto& room : rooms)
    {
        json_rooms.push_back(boost::json::object({
            {"name", room}
        }));
    }
    auto payload = boost::json::serialize(res);

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
    req.push("XADD", room_name, "*", "msg", msg.message);
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
        sess->stream().async_write(boost::asio::buffer(msg.message), yield[ec]);
    }

    return ec;
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

    // Send the rooms the user is in
    send_rooms(ws_, rooms, yield[ec]);
    if (ec)
        return fail(ec, "Sending rooms");

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
        auto msg = boost::json::parse(buffer_to_sv(buff.data()));
        auto type = msg.as_object().at("type").as_string();
        std::cout << "Received message with type: " << type << std::endl;
        const auto& payload = msg.at("payload");

        if (type == "message")
        {
            chat::message parsed_msg = boost::json::value_to<chat::message>(payload);
            ec = process_message(*state_, parsed_msg, yield);
            if (ec)
                return fail(ec, "Broadcasting message");
        }
        // TODO: other mesage types

        // Clear the buffer
        buff.consume(buff.size());
    }
}