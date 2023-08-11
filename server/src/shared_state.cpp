//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "shared_state.hpp"

#include <memory>

#include "session_map.hpp"

using namespace chat;

shared_state::shared_state(std::string doc_root, redis_client redis)
    : impl_{std::move(doc_root), std::move(redis), std::make_unique<session_map>()}
{
}

shared_state::shared_state(shared_state&& rhs) noexcept : impl_(std::move(rhs.impl_)) {}

shared_state& shared_state::operator=(shared_state&& rhs) noexcept
{
    impl_ = std::move(rhs.impl_);
    return *this;
}

shared_state::~shared_state() {}
