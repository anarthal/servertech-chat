#
# Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.16.0)
project(servertech_chat)

find_package(Boost REQUIRED COMPONENTS headers context json)
find_package(OpenSSL REQUIRED)

add_executable(
    main
    src/boost_asio.cpp
    src/boost_beast.cpp
    src/boost_redis.cpp
    src/server/http_session.cpp
    src/server/listener.cpp
    src/server/shared_state.cpp
    src/server/websocket_session.cpp
    src/server/serialization.cpp
    src/server/error.cpp
    src/server/redis_client.cpp
    src/server/websocket.cpp
    src/server/main.cpp
)
target_link_libraries(
    main
    PRIVATE
    Boost::headers
    Boost::context
    Boost::json
    OpenSSL::Crypto
    OpenSSL::SSL
)
target_compile_definitions(
    main
    PRIVATE
    BOOST_BEAST_SEPARATE_COMPILATION
    BOOST_ASIO_SEPARATE_COMPILATION
)
target_precompile_headers(
    main
    PRIVATE
    src/pch.hpp
)