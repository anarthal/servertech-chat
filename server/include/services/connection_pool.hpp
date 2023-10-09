//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CONNECTION_POOL_HPP
#define BOOST_MYSQL_CONNECTION_POOL_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/mysql/tcp.hpp>

#include <memory>
#include <string>

#include "error.hpp"

namespace chat {

struct pool_params
{
    std::string hostname;
    unsigned short port;
    std::string username;
    std::string password;
    std::string database;
    std::size_t initial_size{1};
    std::size_t max_size{150};  // TODO: is this MySQL's max by default?
    // TODO: ssl mode, collation, etc
    // TODO: ssl in general
    // TODO: do we want the user to be able to customize the executor of the new connections?
};

class connection_pool;

class pooled_connection
{
    struct
    {
        connection_pool* pool{};
        void* conn{};
    } impl_;

    friend class connection_pool;
    pooled_connection(connection_pool* pool, void* conn) noexcept : impl_{pool, conn} {}

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
    pooled_connection() noexcept = default;
    pooled_connection(const pooled_connection&) = delete;
    pooled_connection(pooled_connection&& rhs) noexcept : impl_(rhs.impl_) { rhs.impl_ = {}; }
    pooled_connection& operator=(const pooled_connection&) = delete;
    pooled_connection& operator=(pooled_connection&& rhs) noexcept
    {
        std::swap(impl_, rhs.impl_);
        return *this;
    }
    ~pooled_connection();

    bool has_value() const noexcept { return impl_.conn != nullptr; }

    boost::mysql::tcp_connection& get() noexcept { return *ptr(); }
    const boost::mysql::tcp_connection& get() const noexcept { return *const_ptr(); }
    boost::mysql::tcp_connection* operator->() noexcept { return ptr(); }
    const boost::mysql::tcp_connection* operator->() const noexcept { return const_ptr(); }
};

class connection_pool
{
    virtual void return_connection_impl(void* conn) noexcept = 0;

public:
    virtual ~connection_pool() {}
    virtual error_code run(boost::asio::yield_context yield) = 0;
    virtual result<pooled_connection> get_connection(boost::asio::yield_context yield) = 0;
    void return_connection(pooled_connection&& conn) noexcept
    {
        if (conn.has_value())
            return_connection_impl(conn.release());
    }
};

std::unique_ptr<connection_pool> create_connection_pool(
    pool_params&& params,
    boost::asio::any_io_executor ex
);

}  // namespace chat

#endif
