//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_MYSQL_CONNECTION_POOL_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_MYSQL_CONNECTION_POOL_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/mysql/tcp.hpp>

#include <chrono>
#include <memory>
#include <string>

#include "error.hpp"

namespace chat {

struct mysql_pool_params
{
    std::string hostname;
    unsigned short port;
    std::string username;
    std::string password;
    std::string database;
    std::size_t initial_size{1};
    std::size_t max_size{150};  // TODO: is this MySQL's max by default?
    bool multi_queries{false};
    // TODO: ssl mode, collation, etc
    // TODO: ssl in general
    // TODO: do we want the user to be able to customize the executor of the new connections?
};

class mysql_connection_pool;

class mysql_pooled_connection
{
    struct
    {
        mysql_connection_pool* pool{};
        void* conn{};
    } impl_;

    friend class mysql_connection_pool;

    void* release() noexcept
    {
        void* res = impl_.conn;
        impl_ = {};
        return res;
    }

    const boost::mysql::tcp_connection* const_ptr() const noexcept;
    boost::mysql::tcp_connection* ptr() noexcept
    {
        return const_cast<boost::mysql::tcp_connection*>(const_ptr());
    }

public:
    // TODO: hide this
    mysql_pooled_connection(mysql_connection_pool* pool, void* conn) noexcept : impl_{pool, conn} {}
    mysql_pooled_connection() noexcept = default;
    mysql_pooled_connection(const mysql_pooled_connection&) = delete;
    mysql_pooled_connection(mysql_pooled_connection&& rhs) noexcept : impl_(rhs.impl_) { rhs.impl_ = {}; }
    mysql_pooled_connection& operator=(const mysql_pooled_connection&) = delete;
    mysql_pooled_connection& operator=(mysql_pooled_connection&& rhs) noexcept
    {
        std::swap(impl_, rhs.impl_);
        return *this;
    }
    ~mysql_pooled_connection();

    bool has_value() const noexcept { return impl_.conn != nullptr; }

    boost::mysql::tcp_connection& get() noexcept { return *ptr(); }
    const boost::mysql::tcp_connection& get() const noexcept { return *const_ptr(); }
    boost::mysql::tcp_connection* operator->() noexcept { return ptr(); }
    const boost::mysql::tcp_connection* operator->() const noexcept { return const_ptr(); }
};

class mysql_connection_pool
{
    virtual void return_connection_impl(void* conn, bool should_reset) noexcept = 0;

public:
    virtual ~mysql_connection_pool() {}
    virtual error_code run(boost::asio::yield_context yield) = 0;
    virtual void cancel() = 0;
    virtual result_with_message<mysql_pooled_connection> get_connection(
        std::chrono::steady_clock::duration timeout,
        boost::asio::yield_context yield
    ) = 0;
    result_with_message<mysql_pooled_connection> get_connection(boost::asio::yield_context yield)
    {
        return get_connection(std::chrono::seconds(30), yield);
    }
    void return_connection(mysql_pooled_connection&& conn, bool should_reset = true) noexcept
    {
        if (conn.has_value())
            return_connection_impl(conn.release(), should_reset);
    }
};

std::unique_ptr<mysql_connection_pool> create_mysql_connection_pool(
    mysql_pool_params&& params,
    boost::asio::any_io_executor ex
);

}  // namespace chat

#endif
