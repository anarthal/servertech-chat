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
#include <boost/assert.hpp>

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/with_params.hpp>

export module boost.mysql;
import boost.endian;
import boost.charconv;

export namespace boost::mysql {

using mysql::connection_pool;
using mysql::pool_params;
using mysql::pooled_connection;
using mysql::host_and_port;
using mysql::diagnostics;
using mysql::results;
using mysql::static_results;
using mysql::with_params;
using mysql::common_server_errc;

inline auto dont_discard_with_diagnostics(any_connection& c, results& r, error_code& ec)
{
    return c.async_execute("", r, asio::redirect_error(ec));
}

}

module : private;

#define BOOST_IN_MODULE_PURVIEW

extern "C++" {
#include <boost/mysql/src.hpp>    
}
