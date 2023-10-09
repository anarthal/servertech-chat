//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/mysql_connection_pool.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/cancellation_condition.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/tcp.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <list>
#include <memory>
#include <utility>

#include "error.hpp"

// TODO: thread-safety
// TODO: sync functions
// TODO: timeout for get_connection() returning a descriptive error?
// TODO: do we want any logging? or any other hook?

using namespace chat;

using boost::asio::experimental::concurrent_channel;
using boost::asio::experimental::make_parallel_group;
using boost::asio::experimental::wait_for_one;

namespace {

static constexpr std::chrono::seconds resolve_timeout{20};
static constexpr std::chrono::seconds connect_timeout{20};
static constexpr std::chrono::seconds ping_timeout{5};
static constexpr std::chrono::seconds reset_timeout{10};
static constexpr std::chrono::seconds between_retries{10};
static constexpr std::chrono::hours healthcheck_interval{1};

enum class connection_status
{
    pending_connect,
    iddle,
    in_use,
    pending_reset,
    pending_ping,
    pending_close,
};

static constexpr auto rethrow_handler = [](std::exception_ptr ex) {
    if (ex)
        std::rethrow_exception(ex);
};

static error_code sleep(
    boost::asio::steady_timer& timer,
    boost::asio::steady_timer::duration t,
    boost::asio::yield_context yield
)
{
    error_code ec;
    timer.expires_from_now(t);
    timer.async_wait(yield[ec]);
    return ec;
}

static boost::mysql::handshake_params hparams(const mysql_pool_params& input) noexcept
{
    return {
        input.username,
        input.password,
        input.database,
    };
}

static error_with_message do_connect(
    boost::asio::ip::tcp::resolver& resolv,
    boost::mysql::tcp_connection& conn,
    boost::asio::steady_timer& timer,
    const mysql_pool_params& params,
    boost::asio::yield_context yield
)
{
    // Resolve the hostname
    timer.expires_after(resolve_timeout);
    auto gp = make_parallel_group(
        resolv.async_resolve(params.hostname, std::to_string(params.port), boost::asio::deferred),
        timer.async_wait(boost::asio::deferred)
    );
    auto [order, resolv_ec, resolv_entries, timer_ec] = gp.async_wait(wait_for_one(), yield);
    if (order[0] != 0u)
        CHAT_RETURN_ERROR_WITH_MESSAGE(boost::asio::error::operation_aborted, "")
    else if (resolv_ec)
        return {resolv_ec};

    // Connect
    boost::mysql::diagnostics diag;
    timer.expires_after(connect_timeout);
    auto gp2 = make_parallel_group(
        conn.async_connect(*resolv_entries.begin(), hparams(params), diag, boost::asio::deferred),
        timer.async_wait(boost::asio::deferred)
    );
    auto [order2, connect_ec, timer2_ec] = gp2.async_wait(wait_for_one(), yield);
    if (order2[0] != 0u)
        CHAT_RETURN_ERROR_WITH_MESSAGE(boost::asio::error::operation_aborted, "")
    else if (connect_ec)
        return {connect_ec, diag.server_message()};

    // Done
    return {};
}

static error_with_message do_reset(
    boost::mysql::tcp_connection& conn,
    boost::asio::steady_timer& timer,
    boost::asio::yield_context yield
)
{
    boost::mysql::diagnostics diag;
    timer.expires_after(reset_timeout);
    auto gp = make_parallel_group(
        conn.async_reset_connection(diag, boost::asio::deferred),
        timer.async_wait(boost::asio::deferred)
    );
    auto [order, reset_ec, timer_ec] = gp.async_wait(wait_for_one(), yield);
    if (order[0] != 0u)
        CHAT_RETURN_ERROR_WITH_MESSAGE(boost::asio::error::operation_aborted, "")
    else if (reset_ec)
        return {reset_ec, diag.server_message()};
    return {};
}

static error_with_message do_ping(
    boost::mysql::tcp_connection& conn,
    boost::asio::steady_timer& timer,
    boost::asio::yield_context yield
)
{
    boost::mysql::diagnostics diag;
    timer.expires_after(ping_timeout);
    auto gp = make_parallel_group(
        conn.async_ping(diag, boost::asio::deferred),
        timer.async_wait(boost::asio::deferred)
    );
    auto [order, ping_ec, timer_ec] = gp.async_wait(wait_for_one(), yield);
    if (order[0] != 0u)
        CHAT_RETURN_ERROR_WITH_MESSAGE(boost::asio::error::operation_aborted, "")
    else if (ping_ec)
        return {ping_ec, diag.server_message()};
    return {};
}

enum class iddle_wait_result
{
    should_send_ping,
    should_send_connection,
    should_exit
};

static iddle_wait_result iddle_wait(
    boost::asio::steady_timer& timer,
    concurrent_channel<void(error_code)>& reqs,
    boost::asio::yield_context yield
)
{
    timer.expires_after(healthcheck_interval);
    auto gp = make_parallel_group(
        reqs.async_receive(boost::asio::deferred),
        timer.async_wait(boost::asio::deferred)
    );
    auto [order, channel_ec, timer_ec] = gp.async_wait(wait_for_one(), yield);
    if (order[0] == 0u && !channel_ec)
        return iddle_wait_result::should_send_connection;
    else if (order[1] == 0u && !timer_ec)
        return iddle_wait_result::should_send_ping;
    else
        return iddle_wait_result::should_exit;
}

static void do_close(boost::mysql::tcp_connection& conn)
{
    // TODO: there should be a better way to handle this
    auto ex = conn.get_executor();
    conn = boost::mysql::tcp_connection(std::move(ex));
}

static bool was_cancelled(error_code ec) noexcept { return ec == boost::asio::error::operation_aborted; }

struct flag_deleter
{
    void operator()(bool* self) const noexcept { *self = false; }
};
using flag_guard = std::unique_ptr<bool, flag_deleter>;

class connection_node
    : public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>
{
    const mysql_pool_params* params_;
    concurrent_channel<void(error_code)>* requests_;
    concurrent_channel<void(error_code, std::unique_ptr<connection_node>)>* responses_;
    std::size_t* num_connections_;
    boost::asio::ip::tcp::resolver resolv;
    boost::mysql::tcp_connection conn;
    connection_status status;
    boost::asio::steady_timer timer;
    bool task_running_{};

    std::unique_ptr<connection_node> unlink_self() noexcept
    {
        unlink();
        return std::unique_ptr<connection_node>(this);
    }

    void run_connection_task(boost::asio::yield_context yield)
    {
        assert(!task_running_);
        assert(status != connection_status::in_use);
        task_running_ = true;
        flag_guard guard{&task_running_};

        while (true)
        {
            if (status == connection_status::pending_connect)
            {
                auto err = do_connect(resolv, conn, timer, *params_, yield);
                if (err.ec)
                {
                    if (was_cancelled(err.ec))
                        return;
                    status = connection_status::pending_close;
                    err.ec = sleep(timer, between_retries, yield);
                    if (was_cancelled(err.ec))
                        return;
                }
                else
                {
                    status = connection_status::iddle;
                }
            }
            else if (status == connection_status::iddle)
            {
                auto result = iddle_wait(timer, *requests_, yield);
                if (result == iddle_wait_result::should_send_ping)
                {
                    status = connection_status::pending_ping;
                }
                else if (result == iddle_wait_result::should_send_connection)
                {
                    responses_->async_send(error_code(), unlink_self(), yield);
                    return;
                }
                else
                {
                    return;
                }
            }
            else if (status == connection_status::pending_reset)
            {
                auto err = do_reset(conn, timer, yield);
                if (err.ec)
                {
                    if (was_cancelled(err.ec))
                        return;
                    status = connection_status::pending_close;
                }
                else
                {
                    status = connection_status::iddle;
                }
            }
            else if (status == connection_status::pending_ping)
            {
                auto err = do_ping(conn, timer, yield);
                if (err.ec)
                {
                    if (was_cancelled(err.ec))
                        return;
                    status = connection_status::pending_close;
                }
                else
                {
                    status = connection_status::iddle;
                }
            }
            else if (status == connection_status::pending_close)
            {
                do_close(conn);
                status = connection_status::pending_connect;
            }
        }
    }

public:
    connection_node(
        const mysql_pool_params& params,
        concurrent_channel<void(error_code)>& req_chan,
        concurrent_channel<void(error_code, std::unique_ptr<connection_node>)>& res_chan,
        std::size_t& num_connections,
        boost::asio::any_io_executor ex
    )
        : params_(&params),
          requests_(&req_chan),
          responses_(&res_chan),
          num_connections_(&num_connections),
          resolv(ex),
          conn(ex),
          timer(ex),
          status(connection_status::pending_connect)
    {
    }

    connection_node(const connection_node&) = delete;
    connection_node(connection_node&&) = delete;
    connection_node& operator=(const connection_node&) = delete;
    connection_node& operator=(connection_node&&) = delete;
    ~connection_node() { --*num_connections_; }

    void launch_connection_task(boost::asio::any_io_executor ex)
    {
        boost::asio::spawn(
            std::move(ex),
            [this](boost::asio::yield_context yield) mutable { run_connection_task(yield); },
            rethrow_handler  // TODO
        );
    }

    bool is_iddle() const noexcept { return status == connection_status::iddle; }
    void stop_task() { timer.cancel(); }
    void set_status(connection_status new_status) noexcept { status = new_status; }
    const boost::mysql::tcp_connection& get() const noexcept { return conn; }
};

class connection_list
{
    boost::intrusive::list<connection_node, boost::intrusive::constant_time_size<false>> impl_;

public:
    connection_list() = default;
    const auto& get() const noexcept { return impl_; }
    auto& get() noexcept { return impl_; }
    void push_back(std::unique_ptr<connection_node> v) noexcept { impl_.push_back(*v.release()); }
    ~connection_list()
    {
        for (auto& conn : impl_)
            delete &conn;
    }
};

class mysql_connection_pool_impl final : public mysql_connection_pool
{
    std::size_t num_connections_{0};
    bool running_{};
    mysql_pool_params params_;
    connection_list conns_;
    concurrent_channel<void(error_code)> requests_;
    concurrent_channel<void(error_code, std::unique_ptr<connection_node>)> responses_;
    concurrent_channel<void(error_code, std::unique_ptr<connection_node>)> collection_requests_;

    void create_connection()
    {
        std::unique_ptr<connection_node> blk{
            new connection_node(params_, requests_, responses_, num_connections_, requests_.get_executor())};
        blk->launch_connection_task(requests_.get_executor());
        conns_.push_back(std::move(blk));
    }

public:
    mysql_connection_pool_impl(mysql_pool_params&& params, boost::asio::any_io_executor ex)
        : params_(std::move(params)), requests_(ex, 5), responses_(ex), collection_requests_(ex, 5)
    {
    }

    error_code run(boost::asio::yield_context yield) final override
    {
        // Check that we're not already running
        assert(!running_);
        running_ = true;
        flag_guard guard{&running_};

        // Create the initial connections
        conns_.get().clear();
        for (std::size_t i = 0; i < params_.initial_size; ++i)
            create_connection();

        // Listen for collection requests
        error_code ec;
        while (true)
        {
            auto conn = collection_requests_.async_receive(yield[ec]);
            if (ec)
                return ec;
            conn->launch_connection_task(requests_.get_executor());
            conns_.push_back(std::move(conn));
        }
    }

    void cancel() final override
    {
        requests_.close();
        responses_.close();
        collection_requests_.close();
        for (auto& conn : conns_.get())
            conn.stop_task();
    }

    result_with_message<mysql_pooled_connection> get_connection(boost::asio::yield_context yield
    ) final override
    {
        // If there are no available connections but there's room for more, create one.
        // TODO: could we make the internal tasks manage this?
        if (conns_.get().empty() && num_connections_ < params_.max_size)
            create_connection();

        // Send a connection request
        error_code ec;
        requests_.async_send(error_code(), yield[ec]);
        if (ec)
            return error_with_message{ec};

        // Get the response
        auto conn = responses_.async_receive(yield[ec]);
        if (ec)
            return error_with_message{ec};
        return mysql_pooled_connection(this, conn.release());
    }

    void return_connection_impl(void* conn, bool should_reset) noexcept final override
    {
        // TODO: this is gonna be a problem when we try to add thread-safety
        assert(conn != nullptr);
        std::unique_ptr<connection_node> node{static_cast<connection_node*>(conn)};
        node->set_status(should_reset ? connection_status::pending_reset : connection_status::iddle);
        collection_requests_.try_send(error_code(), std::move(node));
    }
};

}  // namespace

mysql_pooled_connection::~mysql_pooled_connection()
{
    if (has_value())
        impl_.pool->return_connection(std::move(*this));
}

const boost::mysql::tcp_connection* mysql_pooled_connection::const_ptr() const noexcept
{
    assert(has_value());
    return &static_cast<connection_node*>(impl_.conn)->get();
}

std::unique_ptr<mysql_connection_pool> chat::create_mysql_connection_pool(
    mysql_pool_params&& params,
    boost::asio::any_io_executor ex
)
{
    return std::make_unique<mysql_connection_pool_impl>(std::move(params), std::move(ex));
}
