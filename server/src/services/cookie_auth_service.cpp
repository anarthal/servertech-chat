//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/cookie_auth_service.hpp"

#include <boost/asio/awaitable.hpp>

#include <algorithm>

#include "error.hpp"
#include "services/mysql_client.hpp"
#include "services/session_store.hpp"
#include "util/cookie.hpp"

using namespace chat;
namespace http = boost::beast::http;
namespace asio = boost::asio;

static constexpr std::string_view session_cookie_name = "sid";
static constexpr std::chrono::seconds session_duration(3600 * 24 * 7);  // 7 days

asio::awaitable<result_with_message<std::string>> cookie_auth_service::generate_session_cookie(
    std::int64_t user_id
)
{
    // Generate a session token
    session_store store{*redis_};
    auto session_id_result = co_await store.generate_session_id(user_id, session_duration);
    if (session_id_result.has_error())
        co_return std::move(session_id_result).error();

    // Generate the cookie to be set
    co_return set_cookie_builder(session_cookie_name, *session_id_result)
        .http_only(true)
        .same_site(same_site_t::strict)
        .max_age(session_duration)
        .build_header();
}

asio::awaitable<result_with_message<std::int64_t>> cookie_auth_service::user_id_from_cookie(
    const boost::beast::http::fields& req
)
{
    // Get the Cookie header from the request
    auto it = req.find(http::field::cookie);
    if (it == req.end())
        CHAT_CO_RETURN_ERROR_WITH_MESSAGE(errc::requires_auth, "")

    // Retrieve the session cookie
    cookie_list cookies(it->value());
    auto cookie_it = std::find_if(cookies.begin(), cookies.end(), [](const cookie_pair& p) {
        return p.name == session_cookie_name;
    });
    if (cookie_it == cookies.end())
        CHAT_CO_RETURN_ERROR_WITH_MESSAGE(errc::requires_auth, "")

    // Look it up in Redis
    session_store store{*redis_};
    auto result = co_await store.get_user_by_session(cookie_it->value);
    if (result.has_error())
    {
        auto err = std::move(result).error();
        if (err.ec == errc::not_found)
            CHAT_CO_RETURN_ERROR_WITH_MESSAGE(errc::requires_auth, std::move(err.msg))
        else
            co_return err;
    }
    co_return result.value();
}

asio::awaitable<result_with_message<user>> cookie_auth_service::user_from_cookie(
    const boost::beast::http::fields& req_headers
)
{
    auto user_id_result = co_await user_id_from_cookie(req_headers);
    if (user_id_result.has_error())
        co_return std::move(user_id_result).error();
    co_return co_await mysql_->get_user_by_id(*user_id_result);
}
