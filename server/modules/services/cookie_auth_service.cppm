//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_COOKIE_AUTH_SERVICE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_COOKIE_AUTH_SERVICE_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/system/result.hpp>

#include <cstdint>

#include "business_types.hpp"

// Contains high-level functions to set and verify user sessions.
// Session IDs are stored in Redis (see session_store.hpp for details).
// The session ID alone is enough to authenticate a client (so it constitutes
// an authentication token).
// Session IDs are transmitted to the client and back using HTTP cookies.

namespace chat {

// Forward declarations
class redis_client;
class mysql_client;

class cookie_auth_service
{
    redis_client* redis_;
    mysql_client* mysql_;

public:
    cookie_auth_service(redis_client& redis, mysql_client& mysql) noexcept : redis_(&redis), mysql_(&mysql) {}

    // Allocates a new session ID for the passed user ID (by storing it in Redis),
    // and returns an appropriate Set-Cookie header.
    boost::asio::awaitable<boost::system::result<std::string>> generate_session_cookie(std::int64_t user_id);

    // Verifies that the user is authenticated via a cookie, returning the user_id of the
    // authenticated user.
    // Returns errc::auth_required if the cookie is not present, invalid,
    // or doesn't match any valid session ID.
    boost::asio::awaitable<boost::system::result<std::int64_t>> user_id_from_cookie(
        const boost::beast::http::fields& req_headers
    );

    // Verifies that the user is authenticated via a cookie, returning the associated user.
    // Works like user_id_from_cookie, but also looks up the user in MySQL.
    boost::asio::awaitable<boost::system::result<user>> user_from_cookie(
        const boost::beast::http::fields& req_headers
    );
};

}  // namespace chat

#endif
