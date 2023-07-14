//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_HTTP_SESSION_HPP
#define BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_HTTP_SESSION_HPP

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/optional/optional.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>

#include <cstddef>
#include <cstdlib>
#include <memory>

#include "error.hpp"
#include "shared_state.hpp"

namespace chat {

class http_session : public boost::enable_shared_from_this<http_session>
{
    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buffer_;
    boost::shared_ptr<shared_state> state_;

    // The parser is stored in an optional container so we can
    // construct it from scratch it at the beginning of each new message.
    boost::optional<boost::beast::http::request_parser<boost::beast::http::string_body>> parser_;

    struct send_lambda;

    void fail(error_code ec, char const* what);
    void do_read();
    void on_read(error_code ec, std::size_t);
    void on_write(error_code ec, std::size_t, bool close);

public:
    http_session(boost::asio::ip::tcp::socket&& socket, const boost::shared_ptr<shared_state>& state);

    void run();
};

}  // namespace chat

#endif
