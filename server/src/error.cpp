//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "error.hpp"

#include <boost/describe/enum.hpp>
#include <boost/describe/enum_to_string.hpp>
#include <boost/system/system_error.hpp>

#include <iostream>
#include <string_view>

namespace chat {

// Adds Boost.Describe metadata to errc. Required for describe::enum_to_string
BOOST_DESCRIBE_ENUM(
    errc,
    redis_parse_error,
    redis_command_failed,
    websocket_parse_error,
    username_exists,
    email_exists,
    not_found,
    invalid_password_hash,
    already_exists,
    requires_auth,
    invalid_base64,
    uncaught_exception,
    invalid_content_type
)

}  // namespace chat

namespace {

static const char* to_string(chat::errc v) noexcept
{
    return boost::describe::enum_to_string(v, "<unknown chat error>");
}

// Custom category for chat::errc. Exposed by get_chat_category
class chat_category final : public boost::system::error_category
{
public:
    const char* name() const noexcept final override { return "chat"; }
    std::string message(int ev) const final override { return to_string(static_cast<chat::errc>(ev)); }
};

static chat_category cat;

}  // namespace

const boost::system::error_category& chat::get_chat_category() noexcept { return cat; }

[[noreturn]] void chat::throw_exception_from_error(const error_with_message& e, const boost::source_location&)
{
    throw boost::system::system_error(e.ec, e.msg);
}

void chat::log_error(error_code ec, std::string_view what, std::string_view diagnostics)
{
    // Don't report on canceled operations
    if (ec == boost::asio::error::operation_aborted)
        return;

    std::cerr << what << ": " << ec << ": " << ec.message();
    if (ec.has_location())
        std::cerr << " (" << ec.location() << ")";
    if (!diagnostics.empty())
        std::cerr << "\nDiagnostics: " << diagnostics;
    std::cerr << '\n';
}