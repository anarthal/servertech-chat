//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/connection_pool.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <boost/mysql/tcp.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <list>
#include <memory>
#include <utility>

#include "error.hpp"

using namespace chat;

using boost::asio::experimental::concurrent_channel;

namespace {

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

// TODO
static error_with_message do_connect(
    boost::mysql::tcp_connection& conn,
    boost::asio::steady_timer& timer,
    const pool_params& params,
    boost::asio::yield_context yield
);

static error_with_message do_reset(
    boost::mysql::tcp_connection& conn,
    boost::asio::steady_timer& timer,
    boost::asio::yield_context yield
);

static error_with_message do_ping(
    boost::mysql::tcp_connection& conn,
    boost::asio::steady_timer& timer,
    boost::asio::yield_context yield
);

static error_code sleep(
    boost::asio::steady_timer& timer,
    boost::asio::steady_timer::duration t,
    boost::asio::yield_context yield
)
{
    timer.expires_from_now(t);
    error_code ec;
    timer.async_wait(yield[ec]);
    return ec;
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
);

static void do_close(boost::mysql::tcp_connection&);

static bool was_cancelled(error_code ec) noexcept { return ec == boost::asio::error::operation_aborted; }

struct flag_deleter
{
    void operator()(bool* self) const noexcept { *self = false; }
};
using flag_guard = std::unique_ptr<bool, flag_deleter>;

class connection_node : public boost::intrusive::list_base_hook<>
{
    const pool_params* params_;
    concurrent_channel<void(error_code)>* requests_;
    concurrent_channel<void(error_code, std::unique_ptr<connection_node>)>* responses_;
    std::size_t* num_connections_;
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
                auto err = do_connect(conn, timer, *params_, yield);
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
        const pool_params& params,
        concurrent_channel<void(error_code)>& req_chan,
        concurrent_channel<void(error_code, std::unique_ptr<connection_node>)>& res_chan,
        std::size_t& num_connections,
        boost::asio::any_io_executor ex
    )
        : params_(&params),
          requests_(&req_chan),
          responses_(&res_chan),
          num_connections_(&num_connections),
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
    void mark_as_pending_reset() noexcept { status = connection_status::pending_reset; }
    const boost::mysql::tcp_connection& get() const noexcept { return conn; }
};

class connection_list
{
    boost::intrusive::list<connection_node> impl_;

public:
    connection_list() = default;
    const boost::intrusive::list<connection_node>& get() const noexcept { return impl_; }
    boost::intrusive::list<connection_node>& get() noexcept { return impl_; }
    void push_back(std::unique_ptr<connection_node> v) noexcept { impl_.push_back(*v.release()); }
    ~connection_list()
    {
        for (auto& conn : impl_)
            delete &conn;
    }
};

class connection_pool_impl final : public connection_pool
{
    std::size_t num_connections_{0};
    bool running_{};
    pool_params params_;
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
    connection_pool_impl(pool_params&& params, boost::asio::any_io_executor ex)
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

    result<pooled_connection> get_connection(boost::asio::yield_context yield) final override
    {
        // If there are no available connections but there's room for more, create one.
        // TODO: could we make the internal tasks manage this?
        if (conns_.get().empty() && num_connections_ < params_.max_size)
            create_connection();

        // Send a connection request
        error_code ec;
        requests_.async_send(error_code(), yield[ec]);
        if (ec)
            return ec;

        // Get the response
        auto conn = responses_.async_receive(yield[ec]);
        if (ec)
            return ec;
        return pooled_connection(this, conn.release());
    }
    void return_connection_impl(void* conn) noexcept final override
    {
        // TODO: this is gonna be a problem when we try to add thread-safety
        assert(conn != nullptr);
        std::unique_ptr<connection_node> node{static_cast<connection_node*>(conn)};
        node->mark_as_pending_reset();
        conns_.push_back(std::move(node));
    }
};

}  // namespace

pooled_connection::~pooled_connection()
{
    if (has_value())
        impl_.pool->return_connection(std::move(*this));
}

const boost::mysql::tcp_connection* pooled_connection::const_ptr() const noexcept
{
    assert(has_value());
    return &static_cast<connection_node*>(impl_.conn)->get();
}

class connection_pool;
