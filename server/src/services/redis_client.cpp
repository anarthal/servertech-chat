//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/redis_client.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/redis/adapter/result.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

#include "error.hpp"
#include "services/redis_serialization.hpp"

using namespace chat;
namespace asio = boost::asio;
namespace redis = boost::redis;

namespace {

class redis_client_impl final : public redis_client
{
    redis::connection conn_;

public:
    redis_client_impl(asio::any_io_executor ex) : conn_(std::move(ex)) {}

    void start_run() final override
    {
        // The host to connect to. Defaults to localhost
        const char* host_c_str = std::getenv("REDIS_HOST");
        std::string host = host_c_str ? host_c_str : "localhost";

        redis::config cfg;
        cfg.addr.host = std::move(host);
        cfg.health_check_interval = std::chrono::seconds::zero();  // Disable health checks for now
        conn_.async_run(cfg, {}, asio::detached);
    }

    void cancel() final override { conn_.cancel(); }

    asio::awaitable<result_with_message<std::vector<message_batch>>> get_room_history(
        std::span<const room_histoy_request> input
    ) final override
    {
        assert(!input.empty());

        // Compose the request. XREVRANGE will get all messages for a room,
        // since the beginning, in reverse order, up to message_batch_size
        redis::request req;
        for (const auto& room_req : input)
        {
            std::string stream_ref = room_req.last_message_id ? "(" : "+";
            if (room_req.last_message_id)
                stream_ref.append(*room_req.last_message_id);
            req.push("XREVRANGE", room_req.room_id, stream_ref, "-", "COUNT", message_batch_size);
        }

        // Run it
        redis::generic_response res;
        error_code ec;
        co_await conn_.async_exec(req, res, asio::redirect_error(ec));
        if (ec)
            co_return error_with_message{ec};

        // Verify success. If any of the nodes contains a Redis error (e.g.
        // because we sent an invalid command), this will contain an error.
        if (res.has_error())
            CHAT_CO_RETURN_ERROR_WITH_MESSAGE(errc::redis_command_failed, std::move(res).error().diagnostic)

        // Parse the response
        auto result = parse_room_history_batch(*res);
        if (result.has_error())
            co_return error_with_message{result.error()};

        // Set the has_more flag
        for (auto& batch : *result)
            batch.has_more = batch.messages.size() >= message_batch_size;

        co_return std::move(*result);
    }

    asio::awaitable<result_with_message<std::vector<std::string>>> store_messages(
        std::string_view room_id,
        std::span<const message> messages
    ) final override
    {
        // Compose the request. This appends a message to the given room and
        // auto-assigns it an ID.
        redis::request req;
        for (const auto& msg : messages)
            req.push("XADD", room_id, "*", "payload", serialize_redis_message(msg));

        // Execute it
        redis::generic_response res;
        error_code ec;
        co_await conn_.async_exec(req, res, asio::redirect_error(ec));
        if (ec)
            co_return error_with_message{ec};

        // Verify success. If any of the nodes contains a Redis error (e.g.
        // because we sent an invalid command), this will contain an error.
        if (res.has_error())
            CHAT_CO_RETURN_ERROR_WITH_MESSAGE(errc::redis_command_failed, std::move(res).error().diagnostic)

        // Parse the response
        auto result = parse_batch_xadd_response(*res);
        if (result.has_error())
            co_return error_with_message{result.error()};
        co_return std::move(*result);
    }

    asio::awaitable<error_with_message> set_nonexisting_key(
        std::string_view key,
        std::string_view value,
        std::chrono::seconds ttl
    ) final override
    {
        // Compose the request. NX prevents key overwrites, EX sets the TTL
        redis::request req;
        req.push("SET", key, value, "NX", "EX", ttl.count());

        // Execute it
        redis::response<std::optional<std::string>> res;
        error_code ec;
        co_await conn_.async_exec(req, res, asio::redirect_error(ec));
        if (ec)
            co_return error_with_message{ec};

        // Check
        auto& result = std::get<0>(res);
        if (result.has_error())
            co_return error_with_message{errc::redis_parse_error, std::move(result).error().diagnostic};
        co_return result.value().has_value() ? error_with_message{}
                                             : error_with_message{errc::already_exists};
    }

    asio::awaitable<result_with_message<std::int64_t>> get_int_key(std::string_view key) final override
    {
        // Compose the request
        redis::request req;
        req.push("GET", key);

        // Execute it
        redis::response<std::optional<std::int64_t>> res;
        error_code ec;
        co_await conn_.async_exec(req, res, asio::redirect_error(ec));
        if (ec)
            co_return error_with_message{ec};

        // Check for errors
        auto& result = std::get<0>(res);
        if (result.has_error())
            co_return error_with_message{errc::redis_parse_error, std::move(result).error().diagnostic};
        auto opt = result.value();

        // Check whether the key was present
        if (opt.has_value())
            co_return opt.value();
        else
            co_return error_with_message{errc::not_found};
    }
};

}  // namespace

std::unique_ptr<redis_client> chat::create_redis_client(asio::any_io_executor ex)
{
    return std::unique_ptr<redis_client>{new redis_client_impl(std::move(ex))};
}