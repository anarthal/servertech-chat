//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/pubsub_service.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/test/unit_test.hpp>

#include <memory>
#include <string>
#include <string_view>

using namespace chat;

BOOST_AUTO_TEST_SUITE(pubsub_service_)

// A subscriber implementation that just records the messages it receives
struct stub_subscriber final : public message_subscriber
{
    std::vector<std::string> messages;

    void on_message(std::string_view message, boost::asio::yield_context yield) override final
    {
        messages.emplace_back(message);
    }
};

std::shared_ptr<stub_subscriber> create_subscriber() { return std::make_shared<stub_subscriber>(); }

using string_vector = std::vector<std::string>;

struct fixture
{
    boost::asio::io_context ctx;
    std::unique_ptr<pubsub_service> pubsub{create_pubsub_service(ctx.get_executor())};
    std::shared_ptr<stub_subscriber> sub1{create_subscriber()};
    std::shared_ptr<stub_subscriber> sub2{create_subscriber()};

    // Cleans previous state, publishes a message, and runs the I/O context
    // so that subscription callbacks are called
    void publish_and_run(std::string_view topic_id, std::string msg)
    {
        sub1->messages.clear();
        sub2->messages.clear();
        pubsub->publish(topic_id, std::move(msg));
        ctx.restart();
        ctx.run();
    }
};

BOOST_FIXTURE_TEST_CASE(publish, fixture)
{
    // Data
    constexpr std::string_view sub1_topics[] = {"r1", "r2"};
    constexpr std::string_view sub2_topics[] = {"r3", "r1"};

    // Subscibe
    pubsub->subscribe(sub1, sub1_topics);
    pubsub->subscribe(sub2, sub2_topics);

    // Publish on topic r1
    publish_and_run("r1", "some message");
    BOOST_TEST(sub1->messages == string_vector{"some message"});
    BOOST_TEST(sub2->messages == string_vector{"some message"});

    // Publish on topic r2
    publish_and_run("r2", "another message");
    BOOST_TEST(sub1->messages == string_vector{"another message"});
    BOOST_TEST(sub2->messages == string_vector{});

    // Publish on topic r3
    publish_and_run("r3", "more messages here!");
    BOOST_TEST(sub1->messages == string_vector{});
    BOOST_TEST(sub2->messages == string_vector{"more messages here!"});

    // Publish to a topic no-one is subscribed to
    publish_and_run("unknown", "this message will get to noone");
    BOOST_TEST(sub1->messages == string_vector{});
    BOOST_TEST(sub2->messages == string_vector{});
}

BOOST_FIXTURE_TEST_CASE(unsubscribe, fixture)
{
    // Data
    constexpr std::string_view topic_ids[] = {"r1", "r2"};

    // Subscribe
    pubsub->subscribe(sub1, topic_ids);

    // Messages are received
    publish_and_run("r1", "some message");
    BOOST_TEST(sub1->messages == string_vector{"some message"});

    // Unsubscribe
    pubsub->unsubscribe(*sub1);

    // Messages are no longer received
    publish_and_run("r1", "some message");
    BOOST_TEST(sub1->messages == string_vector{});
}

// Edge case: we don't crash if we try to remove a subscriber that it's not there
BOOST_FIXTURE_TEST_CASE(remove_session_not_present, fixture)
{
    BOOST_CHECK_NO_THROW(pubsub->unsubscribe(*sub1));
}

// RAII-style subscribe
BOOST_FIXTURE_TEST_CASE(subscribe_guarded, fixture)
{
    // Data
    constexpr std::string_view topic_ids[] = {"r1", "r2"};

    {
        // Subscribe
        auto guard = pubsub->subscribe_guarded(sub1, topic_ids);

        // Messages are received
        publish_and_run("r1", "some message");
        BOOST_TEST(sub1->messages == string_vector{"some message"});
    }

    // After the guard goes out of scope, the subscriber is removed
    publish_and_run("r1", "some message");
    BOOST_TEST(sub1->messages == string_vector{});
}

BOOST_AUTO_TEST_SUITE_END()