//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_USE_NOTHROW_OP_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_USE_NOTHROW_OP_HPP

#include <boost/asio/as_tuple.hpp>
#include <boost/async/op.hpp>

namespace chat {

constexpr auto use_nothrow_op = boost::asio::as_tuple(boost::async::use_op);

}  // namespace chat

#endif
