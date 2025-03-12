//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "api/auth.hpp"
#include "api/api_types.hpp"
#include "request_context.hpp"
#include "services/cookie_auth_service.hpp"
#include "services/mysql_client.hpp"
#include "shared_state.hpp"
#include "util/email.hpp"
#include "util/password_hash.hpp"

using namespace chat;
namespace http = boost::beast::http;
namespace asio = boost::asio;

static constexpr std::size_t min_username_size = 4u;
static constexpr std::size_t max_username_size = 100u;
static constexpr std::size_t max_email_size = 100u;
static constexpr std::size_t min_password_size = 10u;
static constexpr std::size_t max_password_size = 100u;

// Ensure that both username not found and invalid password responses are equal
static response_builder::response_type login_failed(response_builder& resp)
{
    return resp.bad_request_json(api_error_id::login_failed, "Login failed");
}

asio::awaitable<response_builder::response_type> chat::handle_create_account(
    request_context& ctx,
    shared_state& st
)
{
    // Parse params
    auto parse_result = ctx.parse_json_body<create_account_request>();
    if (parse_result.has_error())
        co_return ctx.response().bad_request_json("Invalid body provided");
    const auto& req_params = parse_result.value();

    // Validate params
    if (req_params.username.size() < min_username_size || req_params.username.size() > max_username_size)
        co_return ctx.response().bad_request_json("username: invalid size");
    if (req_params.email.size() > max_email_size)
        co_return ctx.response().bad_request_json("email: too long");
    if (!is_email(req_params.email))
        co_return ctx.response().bad_request_json("email: invalid format");
    if (req_params.password.size() < min_password_size || req_params.password.size() > max_password_size)
        co_return ctx.response().bad_request_json("password: invalid size");

    // Hash the password before insertion. TODO: this is an ultra-expensive
    // computation that should be run in a thread pool.
    // https://github.com/anarthal/servertech-chat/issues/47
    auto hashed_passwd = hash_password(req_params.password);

    // Execute the operation
    auto user_id_result = co_await st.mysql()
                              .create_user(req_params.username, req_params.email, hashed_passwd);

    // Handle errors
    if (user_id_result.has_error())
    {
        auto err = std::move(user_id_result).error();
        if (err.ec == errc::username_exists)
            co_return ctx.response().json_error(http::status::bad_request, api_error_id::username_exists, "");
        else if (err.ec == errc::email_exists)
            co_return ctx.response().json_error(http::status::bad_request, api_error_id::email_exists, "");
        else
            co_return ctx.response().internal_server_error(err);
    }

    // Generate a session cookie
    auto session_cookie_result = co_await st.cookie_auth().generate_session_cookie(*user_id_result);
    if (session_cookie_result.has_error())
        co_return ctx.response().internal_server_error(session_cookie_result.error());
    co_return ctx.response().set_cookie(*session_cookie_result).empty_response();
}

asio::awaitable<response_builder::response_type> chat::handle_login(request_context& ctx, shared_state& st)
{
    // Parse params
    auto parse_result = ctx.parse_json_body<login_request>();
    if (parse_result.has_error())
        co_return ctx.response().bad_request_json("Invalid body provided");
    const auto& req_params = parse_result.value();

    // Validate params
    if (req_params.email.size() > max_email_size)
        co_return ctx.response().bad_request_json("email: too long");
    if (!is_email(req_params.email))
        co_return ctx.response().bad_request_json("email: invalid format");
    if (req_params.password.size() < min_password_size || req_params.password.size() > max_password_size)
        co_return ctx.response().bad_request_json("password: invalid size");

    // Retrieve user by email
    auto user_result = co_await st.mysql().get_user_by_email(req_params.email);

    // Handle errors
    if (user_result.has_error())
    {
        auto err = std::move(user_result).error();
        if (err.ec == errc::not_found)
            co_return login_failed(ctx.response());  // email not found
        else
            co_return ctx.response().internal_server_error(err);
    }
    const auto& user = user_result.value();

    // Verify password. TODO: this function requires a lot of computing,
    // it should be run in a thread pool
    // https://github.com/anarthal/servertech-chat/issues/47
    if (!verify_password(req_params.password, user.hashed_password))
    {
        co_return login_failed(ctx.response());
    }

    // Generate a session cookie
    auto session_cookie_result = co_await st.cookie_auth().generate_session_cookie(user.id);
    if (session_cookie_result.has_error())
        co_return ctx.response().internal_server_error(session_cookie_result.error());
    co_return ctx.response().set_cookie(*session_cookie_result).empty_response();
}