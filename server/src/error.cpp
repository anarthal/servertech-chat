//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "error.hpp"

#include <iostream>

static const char* to_string(chat::errc v) noexcept
{
    switch (v)
    {
    case chat::errc::redis_parse_error: return "redis_parse_error";
    case chat::errc::websocket_parse_error: return "websocket_parse_error";
    default: return "<unknown chat error>";
    }
}

namespace chat {

class chat_category final : public boost::system::error_category
{
public:
    const char* name() const noexcept final override { return "chat"; }
    std::string message(int ev) const final override { return to_string(static_cast<chat::errc>(ev)); }
};

}  // namespace chat

static chat::chat_category cat;

const boost::system::error_category& chat::get_chat_category() noexcept { return cat; }

void chat::log_error(error_code ec, const char* what)
{
    // Don't report on canceled operations
    if (ec == boost::asio::error::operation_aborted)
        return;

    std::cerr << what << ": " << ec << ": " << ec.message();
    if (ec.has_location())
        std::cerr << " (" << ec.location() << ")";
    std::cerr << '\n';
}