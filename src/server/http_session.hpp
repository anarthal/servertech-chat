//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SRC_SERVER_HTTP_SESSION_HPP
#define SERVERTECHCHAT_SRC_SERVER_HTTP_SESSION_HPP

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
