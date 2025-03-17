//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/mysql_client.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/describe/class.hpp>
#include <boost/mysql/any_address.hpp>
#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/with_params.hpp>

#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

#include "business_types.hpp"
#include "business_types_metadata.hpp"  // Required by static_results
#include "error.hpp"

using namespace chat;
namespace mysql = boost::mysql;
namespace asio = boost::asio;

namespace {

// Extracts the diagnostic string from a diagnostics object
std::string get_message(const mysql::diagnostics& diag)
{
    return diag.client_message().empty() ? diag.server_message() : diag.client_message();
}

// Returns the value of an environment variable, or default_value if it's not defined
std::string getenv_or(const char* name, const char* default_value)
{
    const char* res = std::getenv(name);
    return res == nullptr ? default_value : res;
}

// Returns the pool params to use
mysql::pool_params get_pool_params()
{
    return {
        // The server address. We get the hostname from an environment
        // variable, and use the default port.
        .server_address = mysql::host_and_port{getenv_or("MYSQL_HOST", "localhost")},

        // The username to log in as
        .username = getenv_or("MYSQL_USERNAME", "servertech_user"),

        // The password. This one
        .password = getenv_or("MYSQL_PASSWORD", "temp_password"),

        // The database to use
        .database = "servertech_chat",
    };
}

class mysql_client_impl final : public mysql_client
{
    mysql::connection_pool pool_;

public:
    mysql_client_impl(asio::any_io_executor ex) : pool_(std::move(ex), get_pool_params()) {}

    void start_run() override final
    {
        asio::co_spawn(
            pool_.get_executor(),
            [pool = &pool_]() { return pool->async_run(asio::use_awaitable); },
            [](std::exception_ptr exc) {
                if (exc)
                    std::rethrow_exception(exc);
            }
        );
    }

    void cancel() override final { pool_.cancel(); }

    asio::awaitable<result_with_message<std::int64_t>> create_user(
        std::string_view username,
        std::string_view email,
        std::string_view hashed_password
    ) final override
    {
        error_code ec;
        mysql::diagnostics diag;
        mysql::results result;

        // Get a connection
        mysql::pooled_connection conn = co_await pool_.async_get_connection(diag, asio::redirect_error(ec));
        if (ec)
            co_return error_with_message{ec, get_message(diag)};

        // Execute the insertion
        co_await conn->async_execute(
            mysql::with_params(
                "INSERT INTO users (username, email, password) VALUES ({}, {}, {})",
                username,
                email,
                hashed_password
            ),
            result,
            diag,
            asio::redirect_error(ec)
        );

        // Detect duplicates
        if (ec == mysql::common_server_errc::er_dup_entry)
        {
            // As per MySQL documentation, error messages for er_dup_entry
            // are formatted as: Duplicate entry '%s' for key %d
            if (diag.server_message().ends_with("'users.username'"))
                co_return error_with_message{errc::username_exists, ""};
            else if (diag.server_message().ends_with("'users.email'"))
                co_return error_with_message{errc::email_exists, ""};
        }

        // Unknown errors
        if (ec)
            co_return error_with_message{ec, get_message(diag)};

        // Done. MySQL reports last_insert_id as an uint64_t to be able to handle
        // any column type, but our id field is defined as BIGINT (int64).
        // The connection is returned to the pool automatically. The statement
        // is deallocated automatically by the connection pool.
        co_return static_cast<std::int64_t>(result.last_insert_id());
    }

    asio::awaitable<result_with_message<auth_user>> get_user_by_email(std::string_view email) final override
    {
        mysql::diagnostics diag;
        error_code ec;

        // Get a connection
        auto conn = co_await pool_.async_get_connection(diag, asio::redirect_error(ec));
        if (ec)
            co_return error_with_message{ec, get_message(diag)};

        // static_results requires that SQL field names
        // match with C++ struct field names, so we use SQL aliases
        mysql::static_results<auth_user> result;
        co_await conn->async_execute(
            mysql::with_params("SELECT id, password AS hashed_password FROM users WHERE email = {}", email),
            result,
            diag,
            asio::redirect_error(ec)
        );
        if (ec)
            co_return error_with_message{ec, get_message(diag)};

        // Result.
        // The connection is returned to the pool automatically. The statement
        // is deallocated automatically by the connection pool.
        if (result.rows().empty())
            co_return error_with_message{errc::not_found, ""};
        co_return std::move(result.rows()[0]);
    }

    asio::awaitable<result_with_message<user>> get_user_by_id(std::int64_t user_id) final override
    {
        mysql::diagnostics diag;
        error_code ec;

        // Get a connection
        auto conn = co_await pool_.async_get_connection(diag, asio::redirect_error(ec));
        if (ec)
            co_return error_with_message{ec, get_message(diag)};

        // Run the query
        mysql::static_results<user> result;
        co_await conn->async_execute(
            mysql::with_params("SELECT id, username  FROM users WHERE id = {}", user_id),
            result,
            diag,
            asio::redirect_error(ec)
        );
        if (ec)
            co_return error_with_message{ec, get_message(diag)};

        // Result.
        // The connection is returned to the pool automatically. The statement
        // is deallocated automatically by the connection pool.
        if (result.rows().empty())
            co_return error_with_message{errc::not_found, ""};
        co_return std::move(result.rows()[0]);
    }

    asio::awaitable<result_with_message<username_map>> get_usernames(std::span<const std::int64_t> user_ids
    ) final override
    {
        // Check that we have one user ID, at least.
        // Otherwise, the generated query may not be valid.
        if (user_ids.empty())
            co_return result_with_message<username_map>{};

        mysql::diagnostics diag;
        error_code ec;

        // Get a connection
        auto conn = co_await pool_.async_get_connection(diag, asio::redirect_error(ec));
        if (ec)
            co_return error_with_message{ec, get_message(diag)};

        // Execute the query.
        // We can safely do this because we checked that user_ids is not empty.
        // Otherwise, the client-side generated query wouldn't be valid.
        using row_t = std::tuple<std::int64_t, std::string>;
        mysql::static_results<row_t> result;
        co_await conn->async_execute(
            mysql::with_params("SELECT id, username FROM users WHERE id IN ({})", user_ids),
            result,
            diag,
            asio::redirect_error(ec)
        );
        if (ec)
            co_return error_with_message{ec, get_message(diag)};

        // We didn't do anything modifying the connection state, so we can
        // explicitly return it, indicating that no reset is required.
        conn.return_without_reset();

        // Result
        std::unordered_map<std::int64_t, std::string> res;
        for (auto& elm : result.rows())
            res.insert({std::get<0>(elm), std::move(std::get<1>(elm))});

        // Done
        co_return res;
    }
};

}  // namespace

std::unique_ptr<mysql_client> chat::create_mysql_client(asio::any_io_executor ex)
{
    return std::unique_ptr<mysql_client>{new mysql_client_impl(std::move(ex))};
}
