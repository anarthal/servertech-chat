//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/pubsub_service.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/core/span.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <memory>
#include <string>
#include <string_view>

using namespace chat;

namespace {

class pubsub_service_impl final : public pubsub_service
{
    // The type of elements held by our container
    struct subscription
    {
        std::string topic_id;
        std::shared_ptr<message_subscriber> subscriber;

        std::string_view topic_id_sv() const noexcept { return topic_id; }
        const message_subscriber* subscriber_ptr() const noexcept { return subscriber.get(); }
    };

    // We need to efficiently index our container by both topic ID and subscriber
    // identity. We use a Boost.MultiIndex container to maintain such indices,
    // so that both operations run in logarithmic time.
    // This is a multimap-like container, but with logN access times.
    // clang-format off
    using container_type = boost::multi_index::multi_index_container<
        subscription,
        boost::multi_index::indexed_by<
            // Index by topic ID
            boost::multi_index::ordered_non_unique<
                boost::multi_index::const_mem_fun<subscription, std::string_view, &subscription::topic_id_sv>
            >,
            // Index by subscriber identity (comparing pointers)
            boost::multi_index::ordered_non_unique<
                boost::multi_index::const_mem_fun<subscription, const message_subscriber*, &subscription::subscriber_ptr>
            >
        >
    >;
    // clang-format on

    container_type ct_;
    boost::asio::any_io_executor ex_;

public:
    pubsub_service_impl(boost::asio::any_io_executor ex) : ex_(std::move(ex)) {}

    void subscribe(
        std::shared_ptr<message_subscriber> subscriber,
        boost::span<const std::string_view> topic_ids
    ) override final
    {
        // Create a subscription for each requested topic
        for (auto topic_id : topic_ids)
        {
            ct_.insert(subscription{std::string(topic_id), subscriber});
        }
    }

    void unsubscribe(message_subscriber& subscriber) override final
    {
        // Remove any subscription matching the given subscriber
        ct_.get<1>().erase(&subscriber);
    }

    void publish(std::string_view topic_id, std::string message) override final
    {
        // Place the string into a shared object, to avoid making an individual
        // copy per subscription
        auto msg_ptr = std::make_shared<std::string>(std::move(message));

        // Get all subscriptions for this topic
        auto [first, last] = ct_.equal_range(topic_id);

        // Launch the subscriber callbacks in parallel
        for (auto it = first; it != last; ++it)
        {
            boost::asio::spawn(
                ex_,
                [subs = it->subscriber, msg_ptr](boost::asio::yield_context yield) {
                    subs->on_message(*msg_ptr, yield);
                },
                boost::asio::detached
            );
        }
    }
};

}  // namespace

std::unique_ptr<pubsub_service> chat::create_pubsub_service(boost::asio::any_io_executor ex)
{
    return std::unique_ptr<pubsub_service>{new pubsub_service_impl(std::move(ex))};
}