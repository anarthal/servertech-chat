//
// Copyright (c) 2026 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

module;

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>

export module servertech_chat:boost.asio;

export namespace boost::asio {

using asio::any_io_executor;
using asio::as_tuple;
using asio::const_buffer;
using asio::buffer;
using asio::redirect_error;
using asio::use_awaitable;
using asio::detached;
using asio::co_spawn;

namespace experimental {
using experimental::channel;
}

namespace ip {
using ip::tcp;
}

inline asio::awaitable<void> dummy() {
    co_await asio::awaitable<void>{};
}

}