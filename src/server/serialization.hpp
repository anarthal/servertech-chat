// copyright
#ifndef BOOST_MYSQL_
#define BOOST_MYSQL_

#include <boost/redis/resp3/node.hpp>
#include <boost/redis/response.hpp>
#include <boost/variant2/variant.hpp>

#include <string>
#include <string_view>

#include "error.hpp"

namespace chat {

struct message
{
    std::string id;
    std::string username;
    std::string content;
};

using websocket_request = boost::variant2::variant<
    error_code,  // TODO: this should probably include some "reason" for 400-like responses
    message>;

// TODO: can we make this use string_view?
result<std::vector<message>> parse_room_history(const boost::redis::generic_response& from);

std::string serialize_messages_event(const std::vector<chat::message>& messages);
std::string serialize_rooms_event(const std::vector<std::string>& rooms);

websocket_request parse_websocket_request(std::string_view from);

}  // namespace chat

#endif
