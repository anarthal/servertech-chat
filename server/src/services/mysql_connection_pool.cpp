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
#include <boost/asio/experimental/channel.hpp>
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
#include <atomic>
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

using boost::asio::experimental::channel;
using boost::asio::experimental::concurrent_channel;
using boost::asio::experimental::make_parallel_group;
using boost::asio::experimental::wait_for_one;
namespace intrusive = boost::intrusive;

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
    timer_done,
    channel_done,
    cancelled
};

static iddle_wait_result iddle_wait(
    boost::asio::steady_timer& timer,
    concurrent_channel<void(error_code)>& collect_chan,
    boost::asio::yield_context yield
)
{
    timer.expires_after(healthcheck_interval);
    auto gp = make_parallel_group(
        collect_chan.async_receive(boost::asio::deferred),
        timer.async_wait(boost::asio::deferred)
    );
    auto [order, channel_ec, timer_ec] = gp.async_wait(wait_for_one(), yield);
    if (order[0] == 0u && !channel_ec)
        return iddle_wait_result::channel_done;
    else if (order[1] == 0u && !timer_ec)
        return iddle_wait_result::timer_done;
    else
        return iddle_wait_result::cancelled;
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

enum class collection_state
{
    none,
    needs_collect,
    needs_collect_with_reset
};

struct owning_tag;
struct view_tag;

using owning_hook = intrusive::list_base_hook<intrusive::tag<owning_tag>>;
using view_hook = intrusive::list_base_hook<intrusive::tag<view_tag>>;

class iddle_connection_list;

class connection_node : public owning_hook, public view_hook
{
    // Managed by the connection task (this includes list hooks)
    const mysql_pool_params* params_;
    boost::asio::ip::tcp::resolver resolv;
    boost::mysql::tcp_connection conn;
    connection_status status;
    boost::asio::steady_timer timer;
    bool task_running_{};
    iddle_connection_list* iddle_list_;

    // Managed by external entities
    concurrent_channel<void(error_code)> collection_channel_;
    std::atomic<collection_state> collection_state_{collection_state::none};

    void mark_as_iddle() noexcept;
    void remove_from_iddle(connection_status new_status) noexcept;

    void collect(collection_state col_st)
    {
        BOOST_ASSERT(col_st != collection_state::none);
        if (col_st == collection_state::needs_collect_with_reset)
        {
            status = connection_status::pending_reset;
        }
        else
        {
            BOOST_ASSERT(col_st == collection_state::needs_collect);
            mark_as_iddle();
        }
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
                    mark_as_iddle();
                }
            }
            else if (status == connection_status::iddle)
            {
                auto wait_result = iddle_wait(timer, collection_channel_, yield);
                if (wait_result == iddle_wait_result::cancelled)
                    return;
                else if (wait_result == iddle_wait_result::channel_done)
                {
                    // This is a collection request. While we were waiting for
                    // the timer, the user took a connection, used it and returned it.
                    // The connection is taken from the iddle list and its status
                    // is updated externally, not by the connection task.
                    auto col_st = collection_state_.exchange(collection_state::none);
                    BOOST_ASSERT(status == connection_status::in_use);
                    collect(col_st);
                }
                else
                {
                    BOOST_ASSERT(wait_result == iddle_wait_result::timer_done);
                    if (status == connection_status::iddle)
                    {
                        // Time to ping
                        remove_from_iddle(connection_status::pending_ping);
                    }
                    else
                    {
                        // This could happen if the collection notification failed,
                        // or if there was a race condition between the connection
                        // task and the function returning connections to the user
                        BOOST_ASSERT(status == connection_status::in_use);
                        auto col_st = collection_state_.exchange(collection_state::none);
                        if (col_st != collection_state::none)
                        {
                            // Collection request notification failed, collect the connection
                            collect(col_st);
                        }
                        else
                        {
                            // Race condition. Nothing required here - just return to wait
                        }
                    }
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
                    mark_as_iddle();
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
                    mark_as_iddle();
                }
            }
            else if (status == connection_status::pending_close)
            {
                do_close(conn);
                status = connection_status::pending_connect;
            }
        }
    }

    void launch_connection_task(boost::asio::any_io_executor ex)
    {
        boost::asio::spawn(
            std::move(ex),
            [this](boost::asio::yield_context yield) mutable { run_connection_task(yield); },
            rethrow_handler  // TODO
        );
    }

public:
    connection_node(
        const mysql_pool_params& params,
        boost::asio::any_io_executor ex,
        iddle_connection_list& iddle_list
    )
        : params_(&params),
          resolv(ex),
          conn(ex),
          timer(ex),
          status(connection_status::pending_connect),
          iddle_list_(&iddle_list),
          collection_channel_(ex, 1)
    {
        launch_connection_task(std::move(ex));
    }

    void stop_task() { timer.cancel(); }
    const boost::mysql::tcp_connection& get() const noexcept { return conn; }

    void mark_as_in_use() noexcept { remove_from_iddle(connection_status::iddle); }

    void mark_as_collectable(bool should_reset) noexcept
    {
        collection_state_.store(
            should_reset ? collection_state::needs_collect_with_reset : collection_state::needs_collect
        );

        // If, for any reason, this notification fails, the connection will
        // be collected when the next ping is due.
        try
        {
            collection_channel_.try_send(error_code());
        }
        catch (...)
        {
        }
    }
};

class owning_connection_list
{
    intrusive::list<connection_node, intrusive::base_hook<owning_hook>> impl_;

public:
    owning_connection_list() = default;
    const auto& get() const noexcept { return impl_; }
    auto& get() noexcept { return impl_; }
    void push_back(std::unique_ptr<connection_node> v) noexcept { impl_.push_back(*v.release()); }
    std::size_t size() const noexcept { return impl_.size(); }
    std::unique_ptr<connection_node> pop_back() noexcept
    {
        std::unique_ptr<connection_node> res(&impl_.back());
        impl_.pop_back();
        return res;
    }
    ~owning_connection_list()
    {
        for (auto& conn : impl_)
            delete &conn;
    }
};

class iddle_connection_list
{
    intrusive::list<connection_node, intrusive::base_hook<view_hook>> list_;
    channel<void(error_code)> chan_;

public:
    iddle_connection_list(boost::asio::any_io_executor ex) : chan_(std::move(ex)) {}
    connection_node* try_get_one() noexcept
    {
        if (list_.empty())
            return nullptr;
        auto* node = &list_.back();
        node->mark_as_in_use();  // This removes the connection from the list
        return node;
    }

    result<connection_node*> get_one(boost::asio::yield_context yield)
    {
        error_code ec;
        while (true)
        {
            auto* node = try_get_one();
            if (node)
                return node;
            chan_.async_receive(yield[ec]);
            if (ec)
                return ec;
        }
    }

    void add_one(connection_node& node)
    {
        list_.push_back(node);
        chan_.try_send(error_code());
    }

    void close() { chan_.close(); }

    void remove(connection_node& node) { list_.erase(list_.iterator_to(node)); }

    std::size_t size() const noexcept { return list_.size(); }
};

void connection_node::mark_as_iddle() noexcept
{
    status = connection_status::iddle;
    iddle_list_->add_one(*this);
}

void connection_node::remove_from_iddle(connection_status new_status) noexcept
{
    BOOST_ASSERT(status == connection_status::iddle);
    status = new_status;
    iddle_list_->remove(*this);
}

class mysql_connection_pool_impl final : public mysql_connection_pool
{
    bool running_{};
    mysql_pool_params params_;
    owning_connection_list all_conns_;
    iddle_connection_list iddle_conns_;
    std::size_t num_conns_in_use_{};
    boost::asio::any_io_executor ex_;

    void create_connection()
    {
        std::unique_ptr<connection_node> blk{new connection_node(params_, ex_, iddle_conns_)};
        all_conns_.push_back(std::move(blk));
    }

    std::size_t num_managed_connections() const noexcept
    {
        return all_conns_.size() - iddle_conns_.size() - num_conns_in_use_;
    }

    result<connection_node*> get_connection_impl(boost::asio::yield_context yield)
    {
        // Try to get a connection without blocking
        auto* conn = iddle_conns_.try_get_one();
        if (conn)
            return conn;

        // No luck. If there is room for more connections, create one.
        // Don't create new connections if we have other connections in progress
        // of being connected or cleaned - otherwise pool size increases for
        // no reason when there is no connectivity.
        if (all_conns_.size() < params_.max_size && num_managed_connections() == 0u)
            create_connection();

        return iddle_conns_.get_one(yield);
    }

public:
    mysql_connection_pool_impl(mysql_pool_params&& params, boost::asio::any_io_executor ex)
        : params_(std::move(params)), iddle_conns_(ex), ex_(std::move(ex))
    {
    }

    error_code run(boost::asio::yield_context yield) final override
    {
        // TODO: run twice
        // Check that we're not already running
        assert(!running_);
        running_ = true;
        flag_guard guard{&running_};

        // Create the initial connections
        for (std::size_t i = 0; i < params_.initial_size; ++i)
            create_connection();

        // TODO: do we need a way to block this until all connection tasks exit?
        return error_code();
    }

    void cancel() final override
    {
        for (auto& conn : all_conns_.get())
            conn.stop_task();
        iddle_conns_.close();
    }

    result_with_message<mysql_pooled_connection> get_connection(boost::asio::yield_context yield
    ) final override
    {
        // TODO: in thread-safe mode, dispatch to strand

        // Get a connection
        auto node_result = get_connection_impl(yield);
        if (node_result.has_error())
            return error_with_message{node_result.error()};

        // Track that we're using it
        ++num_conns_in_use_;

        // Done
        return mysql_pooled_connection(this, node_result.value());
    }

    void return_connection_impl(void* conn, bool should_reset) noexcept final override
    {
        auto* node = static_cast<connection_node*>(conn);
        node->mark_as_collectable(should_reset);
        --num_conns_in_use_;
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
