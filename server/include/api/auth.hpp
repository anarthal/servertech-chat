//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_API_AUTH_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_API_AUTH_HPP

#include <boost/asio/awaitable.hpp>

#include "request_context.hpp"

// API handler functions for authentication endpoints

namespace chat {

class shared_state;

// POST /create-account
boost::asio::awaitable<response_builder::response_type> handle_create_account(
    request_context& ctx,
    shared_state& st
);

// POST /login
boost::asio::awaitable<response_builder::response_type> handle_login(request_context& ctx, shared_state& st);

}  // namespace chat

#endif
