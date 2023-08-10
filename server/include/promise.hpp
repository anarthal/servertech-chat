//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_PROMISE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_PROMISE_HPP

#include <boost/async/promise.hpp>

namespace chat {

template <class T>
using promise = boost::async::promise<T>;

}  // namespace chat

#endif
