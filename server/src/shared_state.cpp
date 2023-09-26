//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "shared_state.hpp"

#include <boost/asio/any_io_executor.hpp>

#include <memory>

#include "services/cookie_auth_service.hpp"
#include "services/mysql_client.hpp"
#include "services/pubsub_service.hpp"
#include "services/redis_client.hpp"

using namespace chat;

shared_state::shared_state(std::string doc_root, boost::asio::any_io_executor ex)
    : impl_{
          std::move(doc_root),
          create_redis_client(ex),
          create_mysql_client(ex),
          std::make_unique<cookie_auth_service>(redis(), mysql()),
          create_pubsub_service(ex),
      }
{
}

shared_state::shared_state(shared_state&& rhs) noexcept : impl_(std::move(rhs.impl_)) {}

shared_state& shared_state::operator=(shared_state&& rhs) noexcept
{
    impl_ = std::move(rhs.impl_);
    return *this;
}

shared_state::~shared_state() {}
