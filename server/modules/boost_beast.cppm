//
// Copyright (c) 2026 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

module;

// Core
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/beast/core/file_base.hpp>
#include <boost/beast/core/string.hpp>

// HTTP
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>

#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/error.hpp>


export module boost.beast;

import boost.system;

export namespace boost::beast {

// Core
using beast::flat_buffer;
using beast::tcp_stream;
using beast::role_type;
using beast::file_mode;
using beast::iequals;
using beast::string_view;
using beast::make_error_code;
using beast::async_write;

namespace http {
using http::field;
using http::fields;
using http::request;
using http::response;
using http::response_header;
using http::message_generator;
using http::string_body;
using http::empty_body;
using http::file_body;
using http::status;
using http::verb;
using http::error;
using http::request_parser;
using http::async_read;
using http::async_write;
}

namespace websocket {
using websocket::policy_error;
using websocket::is_upgrade;
using websocket::error;
using websocket::make_error_code;
}

}

export namespace chat {

// Required because of GMF discards
inline boost::beast::http::response<boost::beast::http::string_body>
dont_discard_response() { return {}; }

}

module:private;

extern "C++" {
#include <boost/beast/src.hpp>
}
