//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_API_AUTH_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_API_AUTH_HPP

#include <boost/asio/spawn.hpp>

#include "request_context.hpp"

// API handler functions for authentication endpoints

namespace chat {

class shared_state;

// POST /create-account
response_builder::response_type handle_create_account(
    request_context& ctx,
    shared_state& st,
    boost::asio::yield_context yield
);

// POST /login
response_builder::response_type handle_login(
    request_context& ctx,
    shared_state& st,
    boost::asio::yield_context yield
);

}  // namespace chat

#endif
