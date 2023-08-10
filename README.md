# BoostServerTech Chat

This repository holds the code for a chat application written in C++.
This is the first of the [BoostServerTech projects](https://docs.google.com/document/d/1ZQrod1crs8EaNLLqSYIRMacwR3Rv0hC5l-gfL-jOp2M),
a collection of projects that showcase how Boost can be used for server-side code.

## Architecture

* There is a single webserver, written in C++, that handles HTTP and websocket communication.
  The server is single-threaded, asynchronous, and uses Boost.Context coroutines.
  It requires C++17 to build.
* Most client/server communication happen over websocket messages.
* The server uses [Redis streams](https://redis.io/docs/data-types/streams/) to store
  incoming messages from the client. Redis is configured with persistence enabled,
  so no other database is required.
* An in-memory publish/subscribe mechanism is used to broadcast messages to websocket clients.
  This limits the webserver's scalability to a single node - Redis channels will be employed in the future.
* Static files are served by the same webserver that manages 
* The web client uses [next.js](https://nextjs.org/) and [React](https://react.dev/).

## Boost libraries

The server uses the following Boost libraries:

* [Boost.Redis](https://github.com/boostorg/redis) to communicate with Redis.
* [Boost.Beast](http://www.boost.org/libs/beast) for HTTP and websocket communication.
* [Boost.Json](https://boost.org/libs/json) and [Boost.Describe](https://boost.org/libs/describe)
  for message serialization.
* [Boost.MultiIndex](https://boost.org/libs/multi_index) for the in-memory publish/subscribe mechanism.
* [Boost.Variant2](https://boost.org/libs/variant2) for better variants.
* [Boost.Asio](https://boost.org/libs/asio) for networking and coroutines.

## Want to contribute?

Drop us a message in [the cpplang Slack](https://cpplang.slack.com/archives/C05MLSQGA01).
Contributors are more than welcome!