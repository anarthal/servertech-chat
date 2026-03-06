//
// Copyright (c) 2026 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

module;

#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/experimental/channel.hpp>

export module servertech_chat:boost;

export namespace boost::system {

using system::result;
using system::error_code;

}

export namespace boost::asio {

using asio::any_io_executor;

namespace experimental {
using experimental::channel;
}

}