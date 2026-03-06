//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

module servertech_chat:static_files;

import :request_context;
import :shared_state;

namespace chat {

class http_handler;

// Attempts to serve a static file from the document root, based on the passed
// request. Returns 404 if the file can't be found.
response_builder::response_type handle_static_file(request_context& ctx, shared_state& st);

}  // namespace chat
