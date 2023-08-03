//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "shared_state.hpp"

chat::shared_state::shared_state(std::string doc_root, redis_client redis)
    : doc_root_(std::move(doc_root)), redis_(std::move(redis))
{
}
