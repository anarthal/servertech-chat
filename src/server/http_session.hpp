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

#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <optional>

#include "error.hpp"
#include "shared_state.hpp"

namespace chat {

class http_session
{
public:
    using parser_type = boost::beast::http::request_parser<boost::beast::http::string_body>;
    http_session(boost::asio::ip::tcp::socket&& socket, std::shared_ptr<shared_state> state);

    void run(boost::asio::yield_context yield);

private:
    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buffer_;
    std::shared_ptr<shared_state> state_;

    // The parser is stored in an optional container so we can
    // construct it from scratch it at the beginning of each new message.
    std::optional<parser_type> parser_;

    void fail(error_code ec, char const* what);
    void do_read();
    void on_read(error_code ec, std::size_t);
    void on_write(error_code ec, std::size_t, bool close);
};

}  // namespace chat

#endif
