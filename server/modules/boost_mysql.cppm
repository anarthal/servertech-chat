//
// Copyright (c) 2026 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

module;

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/common_server_errc.hpp>
#include <boost/mysql/connection_pool.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/results.hpp>
#include <boost/mysql/static_results.hpp>
#include <boost/mysql/with_params.hpp>

export module boost.mysql;

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

}

module : private;

extern "C++" {
#include <boost/mysql/src.hpp>
}
