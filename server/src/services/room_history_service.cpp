//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/room_history_service.hpp"

#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <array>
#include <string_view>
#include <unordered_set>

#include "business_types.hpp"
#include "services/mysql_client.hpp"
#include "services/redis_client.hpp"

using namespace chat;
namespace asio = boost::asio;
using boost::system::error_code;
using boost::system::result;

static std::vector<std::int64_t> unique_user_ids(const std::vector<message_batch>& input)
{
    std::unordered_set<std::int64_t> set;
    for (const auto& batch : input)
        for (const auto& msg : batch.messages)
            set.insert(msg.user_id);
    return std::vector<std::int64_t>(set.begin(), set.end());
}

asio::awaitable<result<std::pair<std::vector<message_batch>, username_map>>> room_history_service::
    get_room_history(std::span<const std::string_view> room_ids)
{
    // Compose an array of requests for Redis
    std::vector<redis_client::room_histoy_request> redis_req;
    redis_req.resize(room_ids.size());
    for (std::size_t i = 0; i < room_ids.size(); ++i)
        redis_req[i].room_id = room_ids[i];

    // Lookup messages
    auto batches_result = co_await redis_->get_room_history(redis_req);
    if (batches_result.has_error())
        co_return batches_result.error();
    assert(batches_result->size() == room_ids.size());

    // Collect the IDs we need to lookup
    auto user_ids = unique_user_ids(*batches_result);

    // Look them up
    auto usernames_result = co_await mysql_->get_usernames(user_ids);
    if (usernames_result.has_error())
        co_return usernames_result.error();

    co_return std::pair{std::move(*batches_result), std::move(*usernames_result)};
}

asio::awaitable<result<std::pair<message_batch, username_map>>> room_history_service::get_room_history(
    std::string_view room_id
)
{
    // Compose an aray with a single request
    std::array<std::string_view, 1> room_ids{room_id};

    // Call the batch function
    auto res = co_await get_room_history(room_ids);
    if (res.has_error())
        co_return res.error();
    assert(res->first.size() == 1u);

    // Result
    co_return std::pair{std::move(res->first.front()), std::move(res->second)};
}