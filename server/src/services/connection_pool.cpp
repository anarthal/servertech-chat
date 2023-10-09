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
                    status = connection_status::pending_close;
                    if (was_cancelled(err.ec))
                        return;
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
                    status = connection_status::pending_close;
                    if (was_cancelled(err.ec))
                        return;
                }
            }
            else if (status == connection_status::pending_ping)
            {
                auto err = do_ping(conn, timer, yield);
                if (err.ec)
                {
                    status = connection_status::pending_close;
                    if (was_cancelled(err.ec))
                        return;
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

    void create_connection()
    {
        std::unique_ptr<connection_node> blk{
            new connection_node(params_, requests_, responses_, num_connections_, requests_.get_executor())};
        blk->launch_connection_task(requests_.get_executor());
        conns_.push_back(std::move(blk));
    }

public:
    connection_pool_impl(pool_params&& params, boost::asio::any_io_executor ex)
        : params_(std::move(params)), requests_(ex, 5), responses_(ex)
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

        // Launch a task to listen for connection requests
        //    Keep track of the number of requests that are outstanding
        //    If there is room for more connections, create and launch a connection task
        // Launch a task to listen for collection requests
        //    Mark the connection as pending_reset | iddle
        //    Launch a connectio task for it
        // Maybe scan the list of connections periodically, for leaked connections?
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
    }
    void return_connection(pooled_connection&& conn) noexcept final override {}
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

// class pooled_connection_impl
// {
// public:
//     // TODO: mysql is strictly request-reply, so I think this counts as an implicit
//     // strand. Check this assumption.
//     tcp_ssl_connection& get() noexcept { return conn_; }
//     const tcp_ssl_connection& get() const noexcept { return conn_; }

//     inline pooled_connection_impl(connection_pool& pool);
//     pooled_connection_impl(const pooled_connection_impl&) = delete;
//     pooled_connection_impl(pooled_connection_impl&&) = delete;
//     pooled_connection_impl& operator=(const pooled_connection_impl&) = delete;
//     pooled_connection_impl& operator=(pooled_connection_impl&&) = delete;
//     inline ~pooled_connection_impl();

// private:

//     template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
//     BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
//     async_setup(diagnostics&, CompletionToken&& tok);

//     connection_pool* pool_;
//     state_t state_{state_t::not_connected};
//     tcp_ssl_connection conn_;
//     boost::asio::ip::tcp::resolver resolver_;
//     boost::asio::steady_timer timer_;

//     struct setup_op;
//     friend class connection_pool;
//     friend class pooled_connection;
// };

// class pooled_connection;

// class connection_pool
// {
// public:
//     connection_pool(
//         pool_params how_to_connect,
//         boost::asio::any_io_executor exec,
//         std::size_t initial_size,
//         std::size_t max_size
//     )
//         : how_to_connect_(std::move(how_to_connect)), max_size_(max_size), cv_(exec)
//     {
//         // TODO: honor initial_size
//         // for (std::size_t i = 0; i < initial_size; ++i)
//         // {
//         //     add_connection();
//         // }
//     }

//     boost::asio::any_io_executor get_executor() const { return cv_.get_executor(); }

//     template <
//         BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code,
//         ::boost::mysql::pooled_connection))
//             CompletionToken>
//     BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code, pooled_connection))
//     async_get_connection(diagnostics& diag, CompletionToken&& token);

// private:
//     std::shared_ptr<pooled_connection_impl> find_connection()
//     {
//         std::lock_guard<std::mutex> guard(mtx_);

//         // If there are available connections, take one
//         if (!iddle_conns_.empty())
//         {
//             auto conn = iddle_conns_.front();
//             iddle_conns_.pop_front();
//             return conn;
//         }

//         // If the limit allows us, create a new connection
//         if (num_active_conns_ < max_size_)
//         {
//             auto res = std::make_shared<pooled_connection_impl>(*this);
//             ++num_active_conns_;
//             return res;
//         }

//         // No connection available
//         return nullptr;
//     }

//     inline void return_connection(std::shared_ptr<pooled_connection_impl> conn)
//     {
//         std::lock_guard<std::mutex> guard(mtx_);
//         conn->state_ = pooled_connection_impl::state_t::pending_reset;
//         iddle_conns_.push_back(std::move(conn));
//         cv_.notify_one();
//     }

//     void on_connection_destroyed()
//     {
//         std::lock_guard<std::mutex> guard(mtx_);
//         --num_active_conns_;
//         cv_.notify_one();
//     }

//     // Params
//     boost::asio::ssl::context ssl_ctx_;
//     pool_params how_to_connect_;
//     std::size_t max_size_;

//     // State
//     std::mutex mtx_;
//     std::deque<std::shared_ptr<pooled_connection_impl>> iddle_conns_;
//     boost::sam::condition_variable cv_;
//     std::size_t num_active_conns_{0};

//     friend class pooled_connection_impl;
//     friend class pooled_connection;
//     struct get_connection_op;
// };

// class pooled_connection
// {
//     std::shared_ptr<pooled_connection_impl> impl_{};

// public:
//     pooled_connection() = default;
//     pooled_connection(std::shared_ptr<pooled_connection_impl> impl) noexcept : impl_(std::move(impl)) {}
//     pooled_connection(const pooled_connection&) = delete;
//     pooled_connection(pooled_connection&& other) noexcept : impl_(std::move(other.impl_))
//     {
//         other.impl_ = nullptr;
//     }
//     pooled_connection& operator=(const pooled_connection&) = delete;
//     pooled_connection& operator=(pooled_connection&& other) noexcept
//     {
//         std::swap(impl_, other.impl_);
//         return *this;
//     }
//     ~pooled_connection() noexcept
//     {
//         if (impl_)
//         {
//             auto* pool = impl_->pool_;
//             pool->return_connection(std::move(impl_));
//         }
//     }
//     tcp_ssl_connection* operator->() noexcept { return &get(); }
//     const tcp_ssl_connection* operator->() const noexcept { return &get(); }
//     tcp_ssl_connection& get() noexcept { return impl_->conn_; }
//     const tcp_ssl_connection& get() const noexcept { return impl_->conn_; }
// };

// namespace boost {
// namespace mysql {
// namespace detail {

// struct initiate_wait
// {
//     struct cv_op
//     {
//         boost::sam::condition_variable& cv;

//         template <class CompletionToken>
//         BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
//         operator()(CompletionToken&& token)
//         {
//             return cv.async_wait(std::move(token));
//         }
//     };

//     struct timer_op
//     {
//         std::shared_ptr<boost::asio::steady_timer> timer;

//         template <class CompletionToken>
//         BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
//         operator()(CompletionToken&& token)
//         {
//             constexpr std::chrono::seconds wait_timeout{10};
//             timer->expires_after(wait_timeout);
//             return timer->async_wait(std::move(token));
//         }
//     };

//     template <typename CompletionHandler>
//     void operator()(CompletionHandler&& completion_handler, boost::sam::condition_variable& cv) const
//     {
//         struct intermediate_handler
//         {
//             boost::sam::condition_variable& cv_;
//             std::shared_ptr<boost::asio::steady_timer> timer_;
//             typename std::decay<CompletionHandler>::type handler_;

//             // The function call operator matches the completion signature of the
//             // async_write operation.
//             void operator()(
//                 std::array<std::size_t, 2> completion_order,
//                 boost::system::error_code ec1,
//                 boost::system::error_code ec2
//             )
//             {
//                 // Deallocate before calling the handler
//                 timer_.reset();

//                 switch (completion_order[0])
//                 {
//                 case 0: handler_(ec1); break;
//                 case 1: handler_(boost::asio::error::operation_aborted); break;
//                 }

//                 boost::ignore_unused(ec2);
//             }

//             // Preserve executor and allocator
//             using executor_type = boost::asio::associated_executor_t<
//                 typename std::decay<CompletionHandler>::type,
//                 boost::sam::condition_variable::executor_type>;

//             executor_type get_executor() const noexcept
//             {
//                 return boost::asio::get_associated_executor(handler_, cv_.get_executor());
//             }

//             using allocator_type = boost::asio::
//                 associated_allocator_t<typename std::decay<CompletionHandler>::type, std::allocator<void>>;

//             allocator_type get_allocator() const noexcept
//             {
//                 return boost::asio::get_associated_allocator(handler_, std::allocator<void>{});
//             }
//         };

//         auto timer = std::allocate_shared<boost::asio::steady_timer>(
//             boost::asio::get_associated_allocator(completion_handler),
//             cv.get_executor()
//         );
//         boost::asio::experimental::make_parallel_group(cv_op{cv}, timer_op{timer})
//             .async_wait(
//                 boost::asio::experimental::wait_for_one(),
//                 intermediate_handler{
//                     cv,
//                     std::move(timer),
//                     std::forward<CompletionHandler>(completion_handler),
//                 }
//             );
//     }
// };

// template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
// async_wait_for(boost::sam::condition_variable& cv, CompletionToken&& token)
// {
//     return boost::asio::async_initiate<CompletionToken, void(error_code)>(
//         initiate_wait{},
//         token,
//         std::ref(cv)
//     );
// }

// }  // namespace detail
// }  // namespace mysql
// }  // namespace boost

// boost::mysql::pooled_connection_impl::pooled_connection_impl(connection_pool& p)
//     : pool_(&p), conn_(p.get_executor(), p.ssl_ctx_), resolver_(p.get_executor()), timer_(p.get_executor())
// {
// }

// boost::mysql::pooled_connection_impl::~pooled_connection_impl() { pool_->on_connection_destroyed(); }

// // TODO: this strategy should be customizable
// constexpr std::size_t max_num_tries = 2;
// constexpr std::chrono::milliseconds between_tries{1000};

// struct boost::mysql::pooled_connection_impl::setup_op : boost::asio::coroutine
// {
//     pooled_connection_impl& conn_;
//     diagnostics& diag_;
//     std::size_t num_tries_{0};

//     setup_op(pooled_connection_impl& conn, diagnostics& diag) : conn_(conn), diag_(diag) {}

//     template <class Self>
//     void complete(Self& self, error_code ec)
//     {
//         self.complete(ec);
//     }

//     template <class Self>
//     void complete_ok(Self& self)
//     {
//         conn_.state_ = pooled_connection_impl::state_t::in_use;
//         diag_.clear();
//         complete(self, error_code());
//     }

//     template <class Self>
//     void operator()(
//         Self& self,
//         error_code err = {},
//         boost::asio::ip::tcp::resolver::results_type endpoints = {}
//     )
//     {
//         using st_t = pooled_connection_impl::state_t;

//         BOOST_ASIO_CORO_REENTER(*this)
//         {
//             assert(conn_.state_ != st_t::in_use);

//             for (;;)
//             {
//                 if (conn_.state_ == st_t::not_connected)
//                 {
//                     // Resolve endpoints
//                     BOOST_ASIO_CORO_YIELD conn_.resolver_.async_resolve(
//                         conn_.pool_->how_to_connect_.hostname,
//                         conn_.pool_->how_to_connect_.port,
//                         std::move(self)
//                     );
//                     if (err)
//                     {
//                         if (++num_tries_ >= max_num_tries)
//                         {
//                             complete(self, err);
//                             BOOST_ASIO_CORO_YIELD break;
//                         }
//                         conn_.timer_.expires_after(between_tries);
//                         BOOST_ASIO_CORO_YIELD conn_.timer_.async_wait(std::move(self));
//                         if (err)
//                         {
//                             complete(self, err);
//                             BOOST_ASIO_CORO_YIELD break;
//                         }
//                         continue;
//                     }

//                     // connect
//                     BOOST_ASIO_CORO_YIELD conn_.conn_.async_connect(
//                         *endpoints.begin(),
//                         conn_.pool_->how_to_connect_.hparams(),
//                         diag_,
//                         std::move(self)
//                     );
//                     if (err)
//                     {
//                         if (++num_tries_ >= max_num_tries)
//                         {
//                             complete(self, err);
//                             BOOST_ASIO_CORO_YIELD break;
//                         }
//                         conn_.conn_ = tcp_ssl_connection(
//                             conn_.resolver_.get_executor(),
//                             conn_.pool_->ssl_ctx_
//                         );
//                         conn_.timer_.expires_after(between_tries);
//                         BOOST_ASIO_CORO_YIELD conn_.timer_.async_wait(std::move(self));
//                         if (err)
//                         {
//                             complete(self, err);
//                             BOOST_ASIO_CORO_YIELD break;
//                         }
//                         continue;
//                     }

//                     complete_ok(self);
//                     BOOST_ASIO_CORO_YIELD break;
//                 }

//                 if (conn_.state_ == st_t::pending_reset)
//                 {
//                     BOOST_ASIO_CORO_YIELD conn_.conn_.async_reset_session(std::move(self));
//                     if (err)
//                     {
//                         // Close the connection as gracefully as we can. Ignoring any errors on purpose
//                         BOOST_ASIO_CORO_YIELD conn_.conn_.async_close(std::move(self));

//                         // Recreate the connection, since SSL streams can't be reconnected.
//                         // TODO: we could provide a method to reuse the connection's internal buffers while
//                         // recreating the stream
//                         conn_.conn_ = tcp_ssl_connection(
//                             conn_.resolver_.get_executor(),
//                             conn_.pool_->ssl_ctx_
//                         );

//                         // Mark it as initial and retry
//                         conn_.state_ = st_t::not_connected;
//                         if (++num_tries_ >= max_num_tries)
//                         {
//                             complete(self, err);
//                             BOOST_ASIO_CORO_YIELD break;
//                         }
//                         conn_.timer_.expires_after(between_tries);
//                         BOOST_ASIO_CORO_YIELD conn_.timer_.async_wait(std::move(self));
//                         if (err)
//                         {
//                             complete(self, err);
//                             BOOST_ASIO_CORO_YIELD break;
//                         }
//                         continue;
//                     }
//                     complete_ok(self);
//                     BOOST_ASIO_CORO_YIELD break;
//                 }

//                 if (conn_.state_ == st_t::iddle)
//                 {
//                     BOOST_ASIO_CORO_YIELD conn_.conn_.async_ping(std::move(self));
//                     if (err)
//                     {
//                         // Close the connection as gracefully as we can. Ignoring any errors on purpose
//                         BOOST_ASIO_CORO_YIELD conn_.conn_.async_close(std::move(self));

//                         // Recreate the connection, since SSL streams can't be reconnected.
//                         // TODO: we could provide a method to reuse the connection's internal buffers while
//                         // recreating the stream
//                         conn_.conn_ = tcp_ssl_connection(
//                             conn_.resolver_.get_executor(),
//                             conn_.pool_->ssl_ctx_
//                         );

//                         // Mark it as initial and retry
//                         conn_.state_ = st_t::not_connected;
//                         if (++num_tries_ >= max_num_tries)
//                         {
//                             complete(self, err);
//                             BOOST_ASIO_CORO_YIELD break;
//                         }
//                         conn_.timer_.expires_after(between_tries);
//                         BOOST_ASIO_CORO_YIELD conn_.timer_.async_wait(std::move(self));
//                         if (err)
//                         {
//                             complete(self, err);
//                             BOOST_ASIO_CORO_YIELD break;
//                         }
//                         continue;
//                     }
//                     complete_ok(self);
//                     BOOST_ASIO_CORO_YIELD break;
//                 }
//             }
//         }
//     }
// };

// template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code)) CompletionToken>
// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::mysql::error_code))
// boost::mysql::pooled_connection_impl::async_setup(diagnostics& diag, CompletionToken&& token)
// {
//     return boost::asio::async_compose<CompletionToken, void(error_code)>(
//         setup_op(*this, diag),
//         token,
//         resolver_.get_executor()
//     );
// }

// struct boost::mysql::connection_pool::get_connection_op : boost::asio::coroutine
// {
//     connection_pool& pool_;
//     diagnostics& diag_;
//     std::shared_ptr<pooled_connection_impl> conn_{nullptr};

//     get_connection_op(connection_pool& pool, diagnostics& diag) noexcept : pool_(pool), diag_(diag) {}

//     template <class Self>
//     void operator()(Self& self, error_code err = {})
//     {
//         BOOST_ASIO_CORO_REENTER(*this)
//         {
//             while (true)
//             {
//                 // Find a connection we can return to the user
//                 conn_ = pool_.find_connection();
//                 if (conn_)
//                 {
//                     // Setup the connection
//                     BOOST_ASIO_CORO_YIELD conn_->async_setup(diag_, std::move(self));
//                     if (err)
//                     {
//                         self.complete(err, pooled_connection());
//                         BOOST_ASIO_CORO_YIELD break;
//                     }

//                     // Done
//                     self.complete(error_code(), pooled_connection(conn_));
//                     BOOST_ASIO_CORO_YIELD break;
//                 }

//                 // Pool is full and everything is in use - wait
//                 BOOST_ASIO_CORO_YIELD detail::async_wait_for(pool_.cv_, std::move(self));
//             }
//         }
//     }
// };

// template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(::boost::mysql::error_code,
// ::boost::mysql::pooled_connection))
//               CompletionToken>
// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
//     CompletionToken,
//     void(boost::mysql::error_code, boost::mysql::pooled_connection)
// )
// boost::mysql::connection_pool::async_get_connection(diagnostics& diag, CompletionToken&& token)
// {
//     return boost::asio::async_compose<CompletionToken, void(error_code, pooled_connection)>(
//         get_connection_op(*this, diag),
//         token,
//         cv_
//     );
// }
