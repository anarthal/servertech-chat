//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVE_STATIC_FILE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVE_STATIC_FILE_HPP

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/string_body.hpp>

namespace chat {

// Attempts to serve a static file from the document root, based on the passed
// request. Returns a type-erased message generator that can be used to
// write the response back to the client
boost::beast::http::message_generator serve_static_file(
    std::string_view doc_root,
    boost::beast::http::request<boost::beast::http::string_body>&& req
);

}  // namespace chat

#endif
