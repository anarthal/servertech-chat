// copy

#include "serialization.hpp"

#include <boost/describe/class.hpp>
#include <boost/json/fwd.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_to.hpp>

#include <string>
#include <string_view>

#include "error.hpp"

namespace resp3 = boost::redis::resp3;

namespace chat {

struct wire_message
{
    std::string username;
    std::string content;
};
BOOST_DESCRIBE_STRUCT(wire_message, (), (username, content))

}  // namespace chat

chat::result<std::vector<chat::message>> chat::parse_room_history(const boost::redis::generic_response& from)
{
    std::vector<chat::message> res;
    error_code ec;

    // Verify success
    if (from.has_error())
        CHAT_RETURN_ERROR(errc::redis_parse_error)  // TODO: log diagnostics
    const auto& nodes = from.value();

    // Verify this is a list
    auto it = nodes.begin();
    if (it == nodes.end())
        CHAT_RETURN_ERROR(errc::redis_parse_error)
    if (it->data_type != resp3::type::array)
    {
        CHAT_RETURN_ERROR(errc::redis_parse_error)
    }
    ++it;

    // We need a one-pass parser. Data format is:
    // list of MessageEntry:
    //    MessageEntry[0]: string (id)
    //    MessageEntry[1]: list<string> (key-value pairs; always an even number)
    // Since manipulating nodes is cumbersome, we have a single key named "payload",
    // with a single value containing a JSON
    enum state_t
    {
        wants_initial,
        wants_id,
        wants_attr_list,
        wants_key,
        wants_value
    };

    struct parser_data_t
    {
        state_t state{wants_initial};
        const std::string* id{};
    } data;

    for (; it != nodes.end(); ++it)
    {
        if (data.state == wants_initial)
        {
            if (it->data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (it->depth != 1u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (it->aggregate_size != 2u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.state = wants_id;
        }
        else if (data.state == wants_id)
        {
            if (it->data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (it->depth != 2u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.id = &it->value;
            data.state = wants_attr_list;
        }
        else if (data.state == wants_attr_list)
        {
            if (it->data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (it->depth != 2u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (it->aggregate_size != 2u)  // single key/value pair, serialized as JSON
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.state = wants_key;
        }
        else if (data.state == wants_key)
        {
            if (it->data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (it->depth != 3u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (it->value != "payload")
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.state = wants_value;
        }
        else if (data.state == wants_value)
        {
            if (it->data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (it->depth != 3u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)

            // Parse payload
            auto jv = boost::json::parse(it->value, ec);
            if (ec)
                CHAT_RETURN_ERROR(ec)
            auto msg = boost::json::try_value_to<chat::wire_message>(jv);
            if (msg.has_error())
                CHAT_RETURN_ERROR(msg.error())
            res.push_back(chat::message{
                *data.id,
                std::move(msg->username),
                std::move(msg->content),
            });
            data = parser_data_t();
        }
    }

    if (data.state != wants_initial)
        CHAT_RETURN_ERROR(errc::redis_parse_error)

    return res;
}

std::string chat::serialize_messages_event(const std::vector<chat::message>& messages)
{
    // Top-level message
    boost::json::value res{
        {"type",    "messages"          },
        {"payload", boost::json::array()}
    };

    // Individual messages
    auto& payload = res.at("payload").as_array();
    payload.reserve(messages.size());
    for (const auto& msg : messages)
    {
        payload.push_back(boost::json::object({
            {"id",      msg.id      },
            {"user",    msg.username},
            {"content", msg.content },
        }));
    }
    return boost::json::serialize(res);
}

std::string chat::serialize_rooms_event(const std::vector<std::string>& rooms)
{
    boost::json::value res{
        {"type",    "rooms"             },
        {"payload", boost::json::array()}
    };
    auto& payload = res.at("payload").as_array();
    payload.reserve(rooms.size());
    for (const auto& room : rooms)
    {
        payload.push_back(boost::json::object({
            {"name", room}
        }));
    }
    return boost::json::serialize(res);
}

chat::websocket_request chat::parse_websocket_request(std::string_view from)
{
    error_code ec;

    // Parse the JSON
    auto msg = boost::json::parse(from, ec);
    if (ec)
        return ec;

    // Get the message type
    const auto* obj = msg.if_object();
    if (!obj)
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    auto it = obj->find("type");
    if (it == obj->end())
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    const auto& type = it->value();
    std::cout << "Received message with type: " << type << std::endl;  // TODO: remove this

    // Get the payload
    it = obj->find("payload");
    if (it == obj->end())
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    const auto& payload = it->value();

    if (type == "message")
    {
        // Parse the payload
        auto parsed_payload = boost::json::try_value_to<chat::wire_message>(payload);
        if (parsed_payload.has_error())
            CHAT_RETURN_ERROR(parsed_payload.error())
        return chat::message{
            "",  // no ID
            std::move(parsed_payload->username),
            std::move(parsed_payload->content),
        };
    }
    else
    {
        // Unknown typpe
        CHAT_RETURN_ERROR(errc::websocket_parse_error)
    }
}
