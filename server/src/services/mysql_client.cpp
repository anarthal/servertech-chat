//
// Copyright (c) 2023-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/mysql_client.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/describe/class.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connection.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/tcp.hpp>

#include <chrono>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "business_types.hpp"
#include "business_types_metadata.hpp"  // Required by static_results
#include "error.hpp"

using namespace chat;

// DB setup code. This is executed once at startup.
// This should be improved when implementing
// https://github.com/anarthal/servertech-chat/issues/11
// password stores the PHC-formatted password, which upper character
// limit is difficult to get right.
static constexpr std::string_view setup_code = R"SQL(

CREATE DATABASE IF NOT EXISTS servertech_chat;
USE servertech_chat;
CREATE TABLE IF NOT EXISTS users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(100) NOT NULL,
    email VARCHAR(100) NOT NULL,
    password TEXT NOT NULL,
    UNIQUE (username),
    UNIQUE (email)
)

)SQL";

// Returns the handshake params to use.
// This should be improved when implementing
// https://github.com/anarthal/servertech-chat/issues/45
static boost::mysql::handshake_params default_handshake_params() noexcept
{
    return {
        "root",             // user
        "",                 // blank passwd
        "servertech_chat",  // db
    };
}

// Returns the hostname to connect to. Defaults to localhost
static std::string get_mysql_hostname()
{
    const char* host_c_str = std::getenv("MYSQL_HOST");
    return host_c_str ? host_c_str : "localhost";
}

namespace {

// Wraps a MySQL connection.
// Closes the connection asynchronously when destroyed, using the provided yield context.
class guarded_connection
{
public:
    using connection_type = boost::mysql::tcp_connection;

    // Move-only type
    guarded_connection(connection_type&& conn, boost::asio::yield_context yield)
        : impl_{true, std::move(conn), yield}
    {
    }
    guarded_connection(const guarded_connection&) = delete;
    guarded_connection(guarded_connection&& rhs) noexcept : impl_(std::move(rhs.impl_))
    {
        rhs.impl_.has_value = false;
    }
    guarded_connection& operator=(const guarded_connection&) = delete;
    guarded_connection& operator=(guarded_connection&& rhs) noexcept
    {
        std::swap(impl_, rhs.impl_);
        return *this;
    }
    ~guarded_connection()
    {
        if (!impl_.has_value)
            return;

        // Discard any error
        try
        {
            error_code ec;
            impl_.conn.async_close(impl_.yield[ec]);
        }
        catch (...)
        {
        }
    }

    // Retrieves the underlying connection object
    connection_type& get() noexcept { return impl_.conn; }

    // Enables the use of operator->
    connection_type* operator->() noexcept { return &impl_.conn; }

private:
    struct
    {
        // This is required because connection doesn't have any mean to detect moved-from state
        // See https://github.com/boostorg/mysql/issues/177
        bool has_value{};
        connection_type conn;
        boost::asio::yield_context yield;
    } impl_;
};

class mysql_client_impl final : public mysql_client
{
    boost::asio::any_io_executor ex_;
    std::string hostname_;

    // Retrieves a usable MySQL connection.
    // This should be replaced by a connection pool when implementing
    // https://github.com/anarthal/servertech-chat/issues/48
    result_with_message<guarded_connection> get_connection(
        boost::asio::yield_context yield,
        const boost::mysql::handshake_params& hparams = default_handshake_params()
    )
    {
        boost::asio::ip::tcp::resolver resolv(yield.get_executor());
        boost::mysql::tcp_connection conn(yield.get_executor());
        boost::mysql::diagnostics diag;
        error_code ec;

        // Resolve hostname
        auto entries = resolv.async_resolve(hostname_, boost::mysql::default_port_string, yield[ec]);
        if (ec)
            return error_with_message{ec};
        assert(!entries.empty());

        // Connect
        conn.async_connect(*entries.begin(), hparams, diag, yield[ec]);
        if (ec)
            return error_with_message{ec, diag.server_message()};

        return guarded_connection(std::move(conn), yield);
    }

    result_with_message<guarded_connection> get_connection_with_retries(
        boost::asio::yield_context yield,
        const boost::mysql::handshake_params& hparams
    )
    {
        boost::asio::steady_timer timer(yield.get_executor());
        error_code ec;

        while (true)
        {
            // Get a connection
            auto conn_result = get_connection(yield, hparams);
            if (conn_result.has_error())
            {
                // Log the error
                log_error(std::move(conn_result).error(), "Failed to connect to mysql");

                // Wait some time
                timer.expires_from_now(std::chrono::seconds(2));
                timer.async_wait(yield[ec]);

                // If the timer wait errored, it was cancelled, so exit the loop
                if (ec)
                    return error_with_message{ec};
            }
            else
            {
                // We managed to get a connection
                return conn_result;
            }
        }
    }

public:
    mysql_client_impl(boost::asio::any_io_executor ex) : ex_(std::move(ex)), hostname_(get_mysql_hostname())
    {
    }

    error_with_message setup_db(boost::asio::yield_context yield) final override
    {
        error_code ec;
        boost::mysql::diagnostics diag;
        boost::mysql::results result;

        // Handshake params
        auto hparams = default_handshake_params();
        hparams.set_database("");         // may not exist yet
        hparams.set_multi_queries(true);  // enable semicolon-separated queries

        // Get a connection. Perform retries until we get a connection or
        // we get cancelled via Ctrl-C. This provides safety against MySQL
        // failures, which can happen if the server starts before MySQL is ready.
        auto conn_result = get_connection_with_retries(yield, hparams);
        if (conn_result.has_error())
            return std::move(conn_result).error();
        auto& conn = conn_result.value();

        // Run setup code
        conn->async_execute(setup_code, result, diag, yield[ec]);
        if (ec)
            return error_with_message{ec, diag.server_message()};

        // The connection is closed automatically
        return {};
    }

    result_with_message<std::int64_t> create_user(
        std::string_view username,
        std::string_view email,
        std::string_view hashed_password,
        boost::asio::yield_context yield
    ) final override
    {
        boost::mysql::diagnostics diag;
        error_code ec;
        boost::mysql::results result;

        // Get a connection
        auto conn_result = get_connection(yield);
        if (conn_result.has_error())
            return std::move(conn_result).error();
        auto& conn = conn_result.value();

        // Prepare a statement
        constexpr std::string_view
            stmt_sql = R"SQL(INSERT INTO users (username, email, password) VALUES (?, ?, ?))SQL";
        auto stmt = conn->async_prepare_statement(stmt_sql, diag, yield[ec]);
        if (ec)
            return error_with_message{ec, diag.server_message()};

        // Execute it
        conn->async_execute(stmt.bind(username, email, hashed_password), result, diag, yield[ec]);

        // Detect duplicates
        if (ec == boost::mysql::common_server_errc::er_dup_entry)
        {
            // As per MySQL documentation, error messages for er_dup_entry
            // are formatted as: Duplicate entry '%s' for key %d
            if (diag.server_message().ends_with("'users.username'"))
                return error_with_message{errc::username_exists, ""};
            else if (diag.server_message().ends_with("'users.email'"))
                return error_with_message{errc::email_exists, ""};
        }

        // Unknown errors
        if (ec)
            return error_with_message{ec, diag.server_message()};

        // Done. MySQL reports last_insert_id as an uint64_t to be able to handle
        // any column type, but our id field is defined as BIGINT (int64).
        // Closing the statement is not required because we're closing the connection
        // on function exit.
        return static_cast<std::int64_t>(result.last_insert_id());
    }

    result_with_message<auth_user> get_user_by_email(std::string_view email, boost::asio::yield_context yield)
        final override
    {
        boost::mysql::diagnostics diag;
        error_code ec;

        // Get a connection
        auto conn_result = get_connection(yield);
        if (conn_result.has_error())
            return std::move(conn_result).error();
        auto& conn = conn_result.value();

        // Prepare a statement. static_results requires that SQL field names
        // match with C++ struct field names, so we use SQL aliases
        constexpr std::string_view
            stmt_sql = R"SQL(SELECT id, password AS hashed_password FROM users WHERE email = ?)SQL";
        auto stmt = conn->async_prepare_statement(stmt_sql, diag, yield[ec]);
        if (ec)
            return error_with_message{ec, diag.server_message()};

        // Execute it
        boost::mysql::static_results<auth_user> result;
        conn->async_execute(stmt.bind(email), result, diag, yield[ec]);
        if (ec)
            return error_with_message{ec, diag.server_message()};

        // Result.
        // Closing the statement is not required because we're closing the connection
        // on function exit.
        if (result.rows().empty())
            return error_with_message{errc::not_found, ""};
        return std::move(result.rows()[0]);
    }

    result_with_message<user> get_user_by_id(std::int64_t user_id, boost::asio::yield_context yield)
        final override
    {
        boost::mysql::diagnostics diag;
        error_code ec;

        // Get a connection
        auto conn_result = get_connection(yield);
        if (conn_result.has_error())
            return std::move(conn_result).error();
        auto& conn = conn_result.value();

        // Prepare a statement
        constexpr std::string_view stmt_sql = R"SQL(SELECT id, username  FROM users WHERE id = ?)SQL";
        auto stmt = conn->async_prepare_statement(stmt_sql, diag, yield[ec]);
        if (ec)
            return error_with_message{ec, diag.server_message()};

        // Execute it
        boost::mysql::static_results<user> result;
        conn->async_execute(stmt.bind(user_id), result, diag, yield[ec]);
        if (ec)
            return error_with_message{ec, diag.server_message()};

        // Result.
        // Closing the statement is not required because we're closing the connection
        // on function exit.
        if (result.rows().empty())
            return error_with_message{errc::not_found, ""};
        return std::move(result.rows()[0]);
    }

    result_with_message<username_map> get_usernames(
        boost::span<const std::int64_t> user_ids,
        boost::asio::yield_context yield
    ) final override
    {
        if (user_ids.empty())
            return {};

        boost::mysql::diagnostics diag;
        error_code ec;

        // Get a connection
        auto conn_result = get_connection(yield);
        if (conn_result.has_error())
            return std::move(conn_result).error();
        auto& conn = conn_result.value();

        // Compose the SQL statement.
        // WARNING: this is safe only because user IDs are numeric, and not strings.
        // DO NOT USE THIS APPROACH WITH STRINGS, or you will get SQL injections.
        // See https://github.com/boostorg/mysql/issues/69
        std::ostringstream oss;
        oss << "SELECT id, username FROM users WHERE id IN (" << user_ids[0];
        for (auto it = std::next(user_ids.begin()); it != user_ids.end(); ++it)
            oss << "," << *it;
        oss << ')';

        // Execute it
        using row_t = std::tuple<std::int64_t, std::string>;
        boost::mysql::static_results<row_t> result;
        conn->async_execute(oss.str(), result, diag, yield[ec]);
        if (ec)
            return error_with_message{ec, diag.server_message()};

        // Result
        std::unordered_map<std::int64_t, std::string> res;
        for (auto& elm : result.rows())
            res.insert({std::get<0>(elm), std::move(std::get<1>(elm))});
        return res;
    }
};

}  // namespace

std::unique_ptr<mysql_client> chat::create_mysql_client(boost::asio::any_io_executor ex)
{
    return std::unique_ptr<mysql_client>{new mysql_client_impl(std::move(ex))};
}
