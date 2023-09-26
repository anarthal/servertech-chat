//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_MYSQL_CLIENT_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_MYSQL_CLIENT_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/core/span.hpp>
#include <boost/variant2/variant.hpp>

#include <memory>
#include <string>
#include <string_view>

#include "business_types.hpp"
#include "error.hpp"

// A high-level, specialized MySQL client. It implements the operations
// required by our server, abstracting away the actual SQL operations.

namespace chat {

// Using an interface to reduce build times and improve testability
class mysql_client
{
public:
    virtual ~mysql_client() {}

    // Executes DB setup code (DDL statements).
    // Should be called once during application startup.
    virtual error_with_message setup_db(boost::asio::yield_context yield) = 0;

    // Creates a new user object with the given attributes.
    // Returns the ID of the newly created object on success.
    // Retuns errc::username_exists or errc::email_exists if the passed username
    // or email already exist.
    virtual result_with_message<std::int64_t> create_user(
        std::string_view username,
        std::string_view email,
        std::string_view hashed_password,
        boost::asio::yield_context yield
    ) = 0;

    // Retrieves a user's authentication details, given the user's email.
    // Returns errc::not_found if the user doesn't exist.
    virtual result_with_message<auth_user> get_user_by_email(
        std::string_view email,
        boost::asio::yield_context yield
    ) = 0;

    // Retrieves a user by ID.
    // Returns errc::not_found if it doesn't exist.
    virtual result_with_message<user> get_user_by_id(
        std::int64_t user_id,
        boost::asio::yield_context yield
    ) = 0;

    // Retrieves the usernames associated to the passed user_ids.
    // The lookup is performed in batch, for efficiency reasons.
    // If a user ID doesn't exist, it's excluded from the returned map.
    virtual result_with_message<username_map> get_usernames(
        boost::span<const std::int64_t> user_ids,
        boost::asio::yield_context yield
    ) = 0;
};

// Creates a concrete implementation of mysql_client
std::unique_ptr<mysql_client> create_mysql_client(boost::asio::any_io_executor ex);

}  // namespace chat

#endif
