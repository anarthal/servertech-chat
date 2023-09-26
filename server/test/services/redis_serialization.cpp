//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/redis_serialization.hpp"

#include <boost/json/parse.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>
#include <boost/system/error_code.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>

#include "business_types.hpp"

namespace resp3 = boost::redis::resp3;
using namespace chat;

BOOST_AUTO_TEST_SUITE(redis_serialization)

// Creates a node with array type
static resp3::node array_node(std::size_t size, std::size_t depth)
{
    return {resp3::type::array, size, depth, ""};
}

// Creates a node with string type
static resp3::node string_node(std::size_t depth, std::string content)
{
    return {
        resp3::type::blob_string,
        0,
        depth,
        std::move(content),
    };
}

BOOST_AUTO_TEST_CASE(parse_room_history_success)
{
    // Input data
    std::vector<resp3::node> nodes{
        // 1st room top-level node
        array_node(2, 0),
        // first message
        array_node(2, 1),
        string_node(2, "100-1"),
        array_node(2, 2),
        string_node(3, "payload"),
        string_node(3, R"%({"user_id":11,"content":"Test message 2","timestamp":1691666793896})%"),
        // second message
        array_node(2, 1),
        string_node(2, "90-1"),
        array_node(2, 2),
        string_node(3, "payload"),
        string_node(3, R"%({"user_id":12,"content":"Test message 3","timestamp":1691666793897})%"),

        // 2nd room top-level node; room has no messages
        array_node(0, 0),

        // 3rd room top-level node
        array_node(1, 0),
        // first message
        array_node(2, 1),
        string_node(2, "150-1"),
        array_node(2, 2),
        string_node(3, "payload"),
        string_node(3, R"%({"user_id":11,"content":"msg7","timestamp":1691666793898})%"),
    };

    // Call the function
    auto res = parse_room_history_batch(nodes);
    const auto& val = res.value();

    // Validate
    BOOST_TEST(val.size() == 3u);
    BOOST_TEST(val[0].messages.size() == 2u);
    BOOST_TEST(val[1].messages.size() == 0u);
    BOOST_TEST(val[2].messages.size() == 1u);

    BOOST_TEST(val[0].messages[0].id == "100-1");
    BOOST_TEST(val[0].messages[0].user_id == 11);
    BOOST_TEST(serialize_timestamp(val[0].messages[0].timestamp) == 1691666793896);
    BOOST_TEST(val[0].messages[0].content == "Test message 2");

    BOOST_TEST(val[0].messages[1].id == "90-1");
    BOOST_TEST(val[0].messages[1].user_id == 12);
    BOOST_TEST(serialize_timestamp(val[0].messages[1].timestamp) == 1691666793897);
    BOOST_TEST(val[0].messages[1].content == "Test message 3");

    BOOST_TEST(val[2].messages[0].id == "150-1");
    BOOST_TEST(val[2].messages[0].user_id == 11);
    BOOST_TEST(serialize_timestamp(val[2].messages[0].timestamp) == 1691666793898);
    BOOST_TEST(val[2].messages[0].content == "msg7");
}

BOOST_AUTO_TEST_CASE(parse_room_history_empty)
{
    // Input data
    std::vector<resp3::node> nodes{};

    // Call the function
    auto res = parse_room_history_batch(nodes);
    const auto& val = res.value();

    // Validate
    BOOST_TEST(val.size() == 0u);
}

BOOST_AUTO_TEST_CASE(parse_room_history_error)
{
    // Input data
    std::vector<resp3::node> nodes{
        // top-level node
        array_node(1, 0),
        // first message
        array_node(2, 1),
        string_node(2, "100-1"),
        array_node(2, 2),
        array_node(0, 0),  // this top-level node shouldn't be here
        string_node(3, "payload"),
        string_node(
            3,
            R"%({"user":{"id":"user1","username":"username1"},"content":"Test message 2","timestamp":1691666793896})%"
        ),
    };

    // Call the function
    auto res = parse_room_history_batch(nodes);
    BOOST_TEST(res.error() == error_code(errc::redis_parse_error));
}

BOOST_AUTO_TEST_CASE(parse_string_list_success)
{
    // Input data
    std::vector<resp3::node> nodes{string_node(0, "s1"), string_node(0, "s2"), string_node(0, "mykey")};

    // Call the function
    auto res = parse_batch_xadd_response(nodes);
    auto& val = res.value();

    // Validate
    BOOST_TEST(val == std::vector<std::string>({"s1", "s2", "mykey"}));
}

BOOST_AUTO_TEST_CASE(parse_string_list_empty)
{
    // Input data
    std::vector<resp3::node> nodes{};

    // Call the function
    auto res = parse_batch_xadd_response(nodes);
    auto& val = res.value();

    // Validate
    BOOST_TEST(val.size() == 0u);
}

BOOST_AUTO_TEST_CASE(parse_string_list_error)
{
    // Input data
    std::vector<resp3::node> nodes{
        string_node(0, "s1"),
        array_node(1, 0),  // This shouldn't be here
        string_node(0, "s1"),
    };

    // Call the function
    auto res = parse_batch_xadd_response(nodes);
    BOOST_TEST(res.error() == error_code(errc::redis_parse_error));
}

BOOST_AUTO_TEST_CASE(serialize_redis_message_success)
{
    // Input data
    message input{
        "100-10",
        "hello world!",
        timestamp_t{std::chrono::milliseconds(123)},
        11,  // user_id
    };

    // Call the function
    auto output = serialize_redis_message(input);

    // Validate
    const char* expected = R"%({"user_id":11,"content":"hello world!","timestamp":123})%";
    BOOST_TEST(boost::json::parse(output) == boost::json::parse(expected));
}

BOOST_AUTO_TEST_SUITE_END()