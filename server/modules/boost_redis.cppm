//
// Copyright (c) 2026 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

module;

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cerrno>
#include <cfloat>
#include <cinttypes>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <istream>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <version>

#include <boost/redis/resp3/type.hpp>
#include <boost/redis/adapter/result.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

export module boost.redis;

export namespace boost::redis {

namespace resp3 {
using resp3::node;
using resp3::type;
using resp3::is_aggregate;
}

namespace adapter{
using adapter::result;
using adapter::throw_exception_from_error;
}

using redis::connection;
using redis::request;
using redis::config;
using redis::response;
using redis::generic_response;

}

module : private;

#define BOOST_IN_MODULE_PURVIEW

extern "C++" {
#include <boost/redis/src.hpp>
}

