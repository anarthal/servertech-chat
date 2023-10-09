//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/mysql_client.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/experimental/channel.hpp>
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
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "business_types.hpp"
#include "business_types_metadata.hpp"  // Required by static_results
#include "error.hpp"
#include "services/mysql_connection_pool.hpp"

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

// Returns the hostname to connect to. Defaults to localhost
static std::string get_mysql_hostname()
{
    const char* host_c_str = std::getenv("MYSQL_HOST");
    return host_c_str ? host_c_str : "localhost";
}

// Returns the default pool params to use.
// This should be improved when implementing
// https://github.com/anarthal/servertech-chat/issues/45
static mysql_pool_params get_pool_params()
{
    return {
        get_mysql_hostname(),
        boost::mysql::default_port,
        "root",
        "",
        "servertech_chat",
    };
}

namespace {

class mysql_client_impl final : public mysql_client
{
    boost::asio::any_io_executor ex_;
    std::unique_ptr<mysql_connection_pool> pool_;

    // Functions to retrieve a MySQL connection with retries.
    // This is only used for the connection required by setup_db.
    result_with_message<boost::mysql::tcp_connection> get_connection_impl(boost::asio::yield_context yield)
    {
        boost::asio::ip::tcp::resolver resolv(yield.get_executor());
        boost::mysql::tcp_connection conn(yield.get_executor());
        boost::mysql::diagnostics diag;
        boost::mysql::handshake_params hparams{"root", ""};
        hparams.set_multi_queries(true);

        error_code ec;

        // Resolve hostname
        auto entries = resolv
                           .async_resolve(get_mysql_hostname(), boost::mysql::default_port_string, yield[ec]);
        if (ec)
            return error_with_message{ec};
        assert(!entries.empty());

        // Connect
        conn.async_connect(*entries.begin(), hparams, diag, yield[ec]);
        if (ec)
            return error_with_message{ec, diag.server_message()};

        return conn;
    }

    result_with_message<boost::mysql::tcp_connection> get_connection_with_retries(
        boost::asio::yield_context yield
    )
    {
        boost::asio::steady_timer timer(yield.get_executor());
        error_code ec;

        while (true)
        {
            // Get a connection
            auto conn_result = get_connection_impl(yield);
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
    mysql_client_impl(boost::asio::any_io_executor ex)
        : ex_(ex), pool_(create_mysql_connection_pool(get_pool_params(), std::move(ex)))
    {
    }

    error_with_message setup_db(boost::asio::yield_context yield) final override
    {
        error_code ec;
        boost::mysql::diagnostics diag;
        boost::mysql::results result;
        boost::asio::experimental::channel<void(error_code)> chan{ex_};

        // Get a connection. Perform retries until we get a connection or
        // we get cancelled via Ctrl-C. This provides safety against MySQL
        // failures, which can happen if the server starts before MySQL is ready.
        auto conn_result = get_connection_with_retries(yield);
        if (conn_result.has_error())
            return std::move(conn_result).error();
        auto& conn = conn_result.value();

        // Run setup code
        conn.async_execute(setup_code, result, diag, yield[ec]);
        if (ec)
            return error_with_message{ec, diag.server_message()};

        // Close the connection gracefully. Ignore any errors
        conn.async_close(yield[ec]);

        return {};
    }

    void start_run() override final
    {
        boost::asio::spawn(
            ex_,
            [pool = pool_.get()](boost::asio::yield_context yield) { pool->run(yield); },
            [](std::exception_ptr exc) {
                if (exc)
                    std::rethrow_exception(exc);
            }
        );
    }

    void cancel() override final { pool_->cancel(); }

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
        auto conn_result = pool_->get_connection(yield);
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
        // The connection is returned to the pool automatically. The statement
        // is deallocated automatically by the connection pool.
        return static_cast<std::int64_t>(result.last_insert_id());
    }

    result_with_message<auth_user> get_user_by_email(std::string_view email, boost::asio::yield_context yield)
        final override
    {
        boost::mysql::diagnostics diag;
        error_code ec;

        // Get a connection
        auto conn_result = pool_->get_connection(yield);
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
        // The connection is returned to the pool automatically. The statement
        // is deallocated automatically by the connection pool.
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
        auto conn_result = pool_->get_connection(yield);
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
        // The connection is returned to the pool automatically. The statement
        // is deallocated automatically by the connection pool.
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
        auto conn_result = pool_->get_connection(yield);
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

        // We didn't do anything modifying the connection state, so we can
        // explicitly return it, indicating that no reset is required.
        pool_->return_connection(std::move(conn), false);

        // Result
        std::unordered_map<std::int64_t, std::string> res;
        for (auto& elm : result.rows())
            res.insert({std::get<0>(elm), std::move(std::get<1>(elm))});

        // Done
        return res;
    }
};

}  // namespace

std::unique_ptr<mysql_client> chat::create_mysql_client(boost::asio::any_io_executor ex)
{
    return std::unique_ptr<mysql_client>{new mysql_client_impl(std::move(ex))};
}