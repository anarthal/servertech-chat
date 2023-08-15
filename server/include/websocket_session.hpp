//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_WEBSOCKET_SESSION_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_WEBSOCKET_SESSION_HPP

#include <boost/async/channel.hpp>

#include <memory>
#include <string>

#include "error.hpp"
#include "websocket.hpp"

namespace chat {

// Forward declaration
class shared_state;

class websocket_session : public std::enable_shared_from_this<websocket_session>
{
    websocket ws_;
    std::shared_ptr<shared_state> state_;
    boost::async::channel<std::shared_ptr<std::string>> write_channel_;

public:
    websocket_session(websocket socket, std::shared_ptr<shared_state> state);

    promise<void> run();
    promise<void> write(std::shared_ptr<std::string> msg);
};

}  // namespace chat

#endif
