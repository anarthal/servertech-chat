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
#include <string>
#include <utility>

#include "error.hpp"
#include "shared_state.hpp"

// TODO: thread-safety
// TODO: sync functions
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

static boost::mysql::handshake_params hparams(const mysql_pool_params& input) noexcept
{
    return {
        input.username,
        input.password,
        input.database,
    };
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

class connection_node;

class wait_group
{
    std::size_t running_tasks_{};
    channel<void(error_code)> finished_chan_;

    void on_task_finish() noexcept
    {
        if (--running_tasks_ == 0u)
            finished_chan_.try_send(error_code());  // TODO: is this really noexcept?
    }

    struct deleter
    {
        void operator()(wait_group* self) const noexcept { self->on_task_finish(); }
    };

public:
    using guard = std::unique_ptr<wait_group, deleter>;

    wait_group(boost::asio::any_io_executor ex) : finished_chan_(std::move(ex), 1) {}
    guard on_task_start() noexcept
    {
        ++running_tasks_;
        return guard(this);
    }
    void join_tasks(boost::asio::yield_context yield)
    {
        if (running_tasks_)
            finished_chan_.async_receive(yield);
    }
};

class iddle_connection_list
{
    intrusive::list<connection_node, intrusive::base_hook<view_hook>> list_;
    channel<void(error_code)> chan_;
    error_with_message last_error_;

    error_with_message get_last_error() const
    {
        // If we have a specific error, return this. Otherwise, return a generic error
        if (last_error_.ec)
            return last_error_;
        else
            CHAT_RETURN_ERROR_WITH_MESSAGE(boost::asio::error::timed_out, "")
    }

public:
    iddle_connection_list(boost::asio::any_io_executor ex) : chan_(ex, 1) {}

    connection_node* try_get_one() noexcept { return list_.empty() ? nullptr : &list_.back(); }

    result_with_message<connection_node*> get_one(
        std::chrono::steady_clock::duration timeout,
        boost::asio::yield_context yield
    )
    {
        while (true)
        {
            // Try to get a node
            auto* node = try_get_one();
            if (node)
                return node;

            // Wait to be notified, or until a timeout happens
            boost::asio::steady_timer timer(yield.get_executor());
            timer.expires_after(timeout);
            auto gp = make_parallel_group(
                chan_.async_receive(boost::asio::deferred),
                timer.async_wait(boost::asio::deferred)
            );
            auto [order, channel_ec, timer_ec] = gp.async_wait(wait_for_one(), yield);

            // Verify
            if (order[1] == 0u)  // timer finished first
                return get_last_error();
            else if (order[0] == 0u && channel_ec)  // channel was cancelled
                CHAT_RETURN_ERROR_WITH_MESSAGE(boost::asio::error::operation_aborted, "")
            // channel finished first, so try to get the connection
        }
    }

    void add_one(connection_node& node)
    {
        list_.push_back(node);
        chan_.try_send(error_code());
    }

    void close_channel() { chan_.close(); }

    void remove(connection_node& node) { list_.erase(list_.iterator_to(node)); }

    std::size_t size() const noexcept { return list_.size(); }

    void set_last_error(error_with_message value) { last_error_ = std::move(value); }
};

struct error_and_diagnostics
{
    error_code ec;
    boost::mysql::diagnostics diag;
};

// State shared between connection tasks
struct conn_shared_state
{
    wait_group wait_gp;
    iddle_connection_list iddle_list;
    std::size_t num_pending_connections{0};
    error_and_diagnostics last_error;

    conn_shared_state(boost::asio::any_io_executor ex) : wait_gp(ex), iddle_list(std::move(ex)) {}
};

static bool is_pending(connection_status status) noexcept
{
    return status != connection_status::iddle && status != connection_status::in_use;
}

class connection_node : public owning_hook, public view_hook
{
    // Managed by the connection task (this includes list hooks)
    const mysql_pool_params* params_;
    boost::asio::ip::tcp::resolver resolv;
    boost::mysql::tcp_connection conn;
    connection_status status_;
    boost::asio::steady_timer timer;
    conn_shared_state* shared_st_;

    // Managed by external entities
    concurrent_channel<void(error_code)> collection_channel_;
    std::atomic<collection_state> collection_state_{collection_state::none};

    void set_status(connection_status new_status)
    {
        BOOST_ASSERT(new_status != status_);

        // Update the iddle list if required
        if (new_status == connection_status::iddle)
            shared_st_->iddle_list.add_one(*this);
        else if (status_ == connection_status::iddle)
            shared_st_->iddle_list.remove(*this);

        // Update the number of pending connections if required
        if (!is_pending(status_) && is_pending(new_status))
            ++shared_st_->num_pending_connections;
        else if (is_pending(status_) && !is_pending(new_status))
            --shared_st_->num_pending_connections;

        // Update status
        status_ = new_status;
    }

    void collect(collection_state col_st)
    {
        BOOST_ASSERT(col_st != collection_state::none);
        if (col_st == collection_state::needs_collect_with_reset)
        {
            set_status(connection_status::pending_reset);
        }
        else
        {
            BOOST_ASSERT(col_st == collection_state::needs_collect);
            set_status(connection_status::iddle);
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

    error_with_message connect(boost::asio::yield_context yield)
    {
        // Resolve the hostname
        timer.expires_after(resolve_timeout);
        auto gp = make_parallel_group(
            resolv.async_resolve(params_->hostname, std::to_string(params_->port), boost::asio::deferred),
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
            conn.async_connect(*resolv_entries.begin(), hparams(*params_), diag, boost::asio::deferred),
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

    error_code sleep(boost::asio::steady_timer::duration t, boost::asio::yield_context yield)
    {
        error_code ec;
        timer.expires_from_now(t);
        timer.async_wait(yield[ec]);
        return ec;
    }

    error_with_message reset(boost::asio::yield_context yield)
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

    error_with_message ping(boost::asio::yield_context yield)
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

    iddle_wait_result iddle_wait(boost::asio::yield_context yield)
    {
        timer.expires_after(healthcheck_interval);
        auto gp = make_parallel_group(
            collection_channel_.async_receive(boost::asio::deferred),
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

    void close_connection()
    {
        // TODO: there should be a better way to handle this
        auto ex = conn.get_executor();
        conn = boost::mysql::tcp_connection(std::move(ex));
    }

    void run_connection_task(boost::asio::yield_context yield)
    {
        auto task_guard = shared_st_->wait_gp.on_task_start();
        ++shared_st_->num_pending_connections;

        while (true)
        {
            if (status_ == connection_status::pending_connect)
            {
                auto err = connect(yield);
                if (err.ec)
                {
                    if (was_cancelled(err.ec))
                        return;
                    shared_st_->iddle_list.set_last_error(err);
                    err.ec = sleep(between_retries, yield);
                    if (was_cancelled(err.ec))
                        return;
                    set_status(connection_status::pending_close);
                }
                else
                {
                    shared_st_->iddle_list.set_last_error(error_with_message{});
                    set_status(connection_status::iddle);
                }
            }
            else if (status_ == connection_status::iddle)
            {
                auto wait_result = iddle_wait(yield);
                if (wait_result == connection_node::iddle_wait_result::cancelled)
                    return;
                else if (wait_result == connection_node::iddle_wait_result::channel_done)
                {
                    // This is a collection request. While we were waiting for
                    // the timer, the user took a connection, used it and returned it.
                    // The connection is taken from the iddle list and its status
                    // is updated externally, not by the connection task.
                    auto col_st = collection_state_.exchange(collection_state::none);
                    BOOST_ASSERT(status_ == connection_status::in_use);
                    collect(col_st);
                }
                else
                {
                    BOOST_ASSERT(wait_result == iddle_wait_result::timer_done);
                    if (status_ == connection_status::iddle)
                    {
                        // Time to ping
                        set_status(connection_status::pending_ping);
                    }
                    else
                    {
                        // This could happen if the collection notification failed,
                        // or if there was a race condition between the connection
                        // task and the function returning connections to the user
                        BOOST_ASSERT(status_ == connection_status::in_use);
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
            else if (status_ == connection_status::pending_reset)
            {
                auto err = reset(yield);
                if (err.ec)
                {
                    if (was_cancelled(err.ec))
                        return;
                    set_status(connection_status::pending_close);
                }
                else
                {
                    set_status(connection_status::iddle);
                }
            }
            else if (status_ == connection_status::pending_ping)
            {
                auto err = ping(yield);
                if (err.ec)
                {
                    if (was_cancelled(err.ec))
                        return;
                    set_status(connection_status::pending_close);
                }
                else
                {
                    set_status(connection_status::iddle);
                }
            }
            else if (status_ == connection_status::pending_close)
            {
                close_connection();
                set_status(connection_status::pending_connect);
            }
        }
    }

public:
    connection_node(
        const mysql_pool_params& params,
        boost::asio::any_io_executor ex,
        conn_shared_state& shared_st
    )
        : params_(&params),
          resolv(ex),
          conn(ex),
          timer(ex),
          status_(connection_status::pending_connect),
          shared_st_(&shared_st),
          collection_channel_(ex, 1)
    {
        launch_connection_task(std::move(ex));
    }

    ~connection_node() { printf("Dtor\n"); }

    void stop_task()
    {
        timer.cancel();
        collection_channel_.close();
    }
    const boost::mysql::tcp_connection& get() const noexcept { return conn; }

    void mark_as_in_use() noexcept { set_status(connection_status::in_use); }

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

class mysql_connection_pool_impl final : public mysql_connection_pool
{
    bool running_{};
    mysql_pool_params params_;
    owning_connection_list all_conns_;
    conn_shared_state shared_st_;
    boost::asio::any_io_executor ex_;

    void create_connection()
    {
        std::unique_ptr<connection_node> node{new connection_node(params_, ex_, shared_st_)};
        all_conns_.push_back(std::move(node));
    }

    result_with_message<connection_node*> get_connection_impl(
        std::chrono::steady_clock::duration timeout,
        boost::asio::yield_context yield
    )
    {
        // Try to get a connection without blocking
        auto* conn = shared_st_.iddle_list.try_get_one();
        if (conn)
            return conn;

        // No luck. If there is room for more connections, create one.
        // Don't create new connections if we have other connections pending
        // (i.e. being connected, reset... ) - otherwise pool size increases for
        // no reason when there is no connectivity.
        if (all_conns_.size() < params_.max_size && shared_st_.num_pending_connections == 0u)
            create_connection();

        // Wait for a connection to become iddle and return it
        return shared_st_.iddle_list.get_one(timeout, yield);
    }

public:
    mysql_connection_pool_impl(mysql_pool_params&& params, boost::asio::any_io_executor ex)
        : params_(std::move(params)), shared_st_(ex), ex_(std::move(ex))
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

        // Wait for all connection tasks to exit - this should happen
        // after cancel() is called
        shared_st_.wait_gp.join_tasks(yield);

        // Done
        return error_code();
    }

    void cancel() final override
    {
        for (auto& conn : all_conns_.get())
            conn.stop_task();
        shared_st_.iddle_list.close_channel();
    }

    result_with_message<mysql_pooled_connection> get_connection(
        std::chrono::steady_clock::duration timeout,
        boost::asio::yield_context yield
    ) final override
    {
        // TODO: in thread-safe mode, dispatch to strand

        // Get a connection
        auto node_result = get_connection_impl(timeout, yield);
        if (node_result.has_error())
            return std::move(node_result).error();
        auto* node = node_result.value();

        // Mark the node as in use
        node->mark_as_in_use();

        // Done
        return mysql_pooled_connection(this, node);
    }

    void return_connection_impl(void* conn, bool should_reset) noexcept final override
    {
        auto* node = static_cast<connection_node*>(conn);
        node->mark_as_collectable(should_reset);
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
