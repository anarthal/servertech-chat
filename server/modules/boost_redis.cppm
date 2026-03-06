//
// Copyright (c) 2026 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

module;

#include <boost/redis/resp3/node.hpp>
#include <boost/redis/connection.hpp>

export module servertech_chat:boost.redis;

export namespace boost::redis {

namespace resp3 {
using resp3::node;
using resp3::type;
}

using redis::connection;

}
