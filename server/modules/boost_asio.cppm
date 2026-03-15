//
// Copyright (c) 2026 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

module;

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ssl/stream.hpp>

export module boost.asio;

export namespace boost::asio {

using asio::any_io_executor;
using asio::as_tuple;
using asio::awaitable;
using asio::buffer;
using asio::cancel_after;
using asio::co_spawn;
using asio::const_buffer;
using asio::detached;
using asio::io_context;
using asio::redirect_error;
using asio::signal_set;
using asio::socket_base;
using asio::steady_timer;
using asio::use_awaitable;

namespace experimental {
using experimental::channel;
}

namespace ip {
using ip::tcp;
using ip::make_address;
}

namespace this_coro {
using this_coro::executor;
}

namespace ssl {
using ssl::stream;
}

}

export namespace chat {

// Required because GMF discards + std::coroutine_traits
inline boost::asio::awaitable<void> dont_discard_awaitable_coroutine_traits() {
    co_await boost::asio::awaitable<void>{};
}

}
