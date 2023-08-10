//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_HTTP_SESSION_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_HTTP_SESSION_HPP

#include <cstddef>
#include <memory>

#include "promise.hpp"
#include "shared_state.hpp"

namespace chat {

promise<void> run_http_session(boost::asio::ip::tcp::socket&& socket, std::shared_ptr<shared_state> state);

}  // namespace chat

#endif
