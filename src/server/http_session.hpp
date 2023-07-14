//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_HTTP_SESSION_HPP
#define BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_HTTP_SESSION_HPP

#include <boost/asio/spawn.hpp>

#include <cstddef>
#include <memory>

#include "shared_state.hpp"

namespace chat {

void run_http_session(
    boost::asio::ip::tcp::socket&& socket,
    std::shared_ptr<shared_state> state,
    boost::asio::yield_context yield
);

}  // namespace chat

#endif
