//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_STATIC_FILES_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_STATIC_FILES_HPP

#include "request_context.hpp"
#include "shared_state.hpp"

namespace chat {

class http_handler;

// Attempts to serve a static file from the document root, based on the passed
// request. Returns 404 if the file can't be found.
response_builder::response_type handle_static_file(request_context& ctx, shared_state& st);

}  // namespace chat

#endif
