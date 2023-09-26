//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_PUBSUB_SERVICE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_PUBSUB_SERVICE_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/core/span.hpp>

#include <memory>
#include <string>
#include <string_view>

#include "error.hpp"

// An in-memory publish-subscribe mechanism. Used to broadcast messages between clients.

namespace chat {

// Any subscriber must implement this interface
class message_subscriber
{
public:
    virtual ~message_subscriber() {}

    // Called when a message is received. The function is run in its own coroutine,
    // and gets passed a yield_context, allowing for async code within it.
    virtual void on_message(std::string_view message, boost::asio::yield_context yield) = 0;
};

// This is an interface to reduce compile times.
class pubsub_service
{
    // Implementation of subscribe_guarded
    struct subscriber_deleter
    {
        pubsub_service& self;

        void operator()(message_subscriber* subscriber) const noexcept { self.unsubscribe(*subscriber); }
    };

public:
    virtual ~pubsub_service() {}

    // Subscribes a subscriber object to the given topic IDs. When a message
    // for any of these topics is received (via a call to publish),
    // message_subscriber::on_message will be called.
    virtual void subscribe(
        std::shared_ptr<message_subscriber> subscriber,
        boost::span<const std::string_view> topic_ids
    ) = 0;

    // Removes all subscriptions for the given subscriber.
    // Subscriptions are matched by subscriber identity (i.e. pointer comparison).
    // If the subscriber doesn't exist, the function is a no-op.
    virtual void unsubscribe(message_subscriber& subscriber) = 0;

    // Publishes a message to the given topic.
    // All subscribers are notified in parallel, each getting its own coroutine.
    virtual void publish(std::string_view topic_id, std::string message) = 0;

    // RAII-style subscribe. When the guard is destroyed, the subscription is removed.
    using subscriber_guard = std::unique_ptr<message_subscriber, subscriber_deleter>;
    subscriber_guard subscribe_guarded(
        std::shared_ptr<message_subscriber> subscriber,
        boost::span<const std::string_view> topic_ids
    )
    {
        auto* ptr = subscriber.get();
        subscribe(std::move(subscriber), topic_ids);
        return subscriber_guard(ptr, subscriber_deleter{*this});
    }
};

// Create a concrete pubsub_service. The executor is used to launch the coroutines
// where subscribe callbacks run.
std::unique_ptr<pubsub_service> create_pubsub_service(boost::asio::any_io_executor ex);

}  // namespace chat

#endif
