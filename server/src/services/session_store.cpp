//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/session_store.hpp"

#include <array>
#include <openssl/rand.h>
#include <stdexcept>
#include <string>
#include <string_view>

#include "error.hpp"
#include "services/redis_client.hpp"
#include "util/base64.hpp"

using namespace chat;

static constexpr std::size_t session_id_size = 16;  // bytes

static std::string generate_identifier()
{
    // Generate a random session ID. This uses the public random generator
    // because this value is exposed to the user
    std::array<unsigned char, session_id_size> sid{};
    int ec = RAND_bytes(sid.data(), sid.size());
    if (ec <= 0)
        throw std::runtime_error("Generating session ID: RAND_bytes");

    // base64 encode the session ID so it can be transmitted
    // using a cookie or stored in Redis
    return base64_encode(sid);
}

static std::string get_redis_key(std::string_view session_id)
{
    constexpr std::string_view prefix = "session_";

    std::string res;
    res.reserve(prefix.size() + session_id.size());
    res += prefix;
    res += session_id;
    return res;
}

using namespace chat;

result_with_message<std::string> session_store::generate_session_id(
    std::int64_t user_id,
    std::chrono::seconds session_duration,
    boost::asio::yield_context yield
)
{
    // Convert the user ID to string
    auto user_id_str = std::to_string(user_id);

    while (true)
    {
        // Generate an identifier
        auto id = generate_identifier();
        auto redis_key = get_redis_key(id);

        // Try to insert it
        auto err = redis_->set_nonexisting_key(redis_key, user_id_str, session_duration, yield);

        // If we were successful, done. If we got a conflict (unlikely), generate a new ID.
        // Exit on unknown errors
        if (!err.ec)
            return id;
        else if (err.ec != errc::already_exists)
            return err;
    }
}

result_with_message<std::int64_t> session_store::get_user_by_session(
    std::string_view session_id,
    boost::asio::yield_context yield
)
{
    auto redis_key = get_redis_key(session_id);
    return redis_->get_int_key(redis_key, yield);
}