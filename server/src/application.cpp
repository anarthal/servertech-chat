//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "application.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/redis/connection.hpp>

#include <memory>

#include "error.hpp"
#include "listener.hpp"
#include "redis_client.hpp"
#include "shared_state.hpp"

struct chat::application::impl
{
    boost::asio::io_context ioc;
    listener list;
    std::shared_ptr<shared_state> st;  // TODO: do we really need this to be shared?
    boost::asio::ip::tcp::endpoint listening_endpoint;
    boost::asio::signal_set signals;

    impl(application_config&& config)
        : ioc(1),  // single-threaded
          list(ioc.get_executor()),
          st(std::make_shared<shared_state>(std::move(config.doc_root), redis_client(ioc.get_executor()))),
          listening_endpoint(boost::asio::ip::make_address(config.ip), config.port),
          signals(ioc.get_executor(), SIGINT, SIGTERM)
    {
    }
};

chat::application::application(application_config config) : impl_(new impl(std::move(config))) {}

chat::application::application(application&& rhs) noexcept : impl_(std::move(rhs.impl_)) {}

chat::application& chat::application::operator=(application&& rhs) noexcept
{
    impl_ = std::move(rhs.impl_);
    return *this;
}

chat::application::~application() {}

chat::error_code chat::application::setup() { return impl_->list.setup(impl_->listening_endpoint); }

void chat::application::run_until_completion(bool with_signal_handlers)
{
    // Launch the Redis connection
    impl_->st->redis().start_run();

    // Start listening for HTTP connections. This will run until the context is stopped
    impl_->list.run_until_completion(impl_->st);

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    if (with_signal_handlers)
    {
        impl_->signals.async_wait([this](error_code, int) { this->stop(); });
    }

    // Run the io_context
    impl_->ioc.run();
}

void chat::application::stop()
{
    // Stop the Redis reconnection loop
    impl_->st->redis().cancel();

    // Stop the io_context. This will cause run() to return
    impl_->ioc.stop();
}
