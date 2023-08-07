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

# This library is consumed by the actual server and the tests
add_library(
    servertech_chat
    STATIC
    src/boost_asio.cpp
    src/boost_beast.cpp
    src/boost_redis.cpp
    src/http_session.cpp
    src/listener.cpp
    src/shared_state.cpp
    src/websocket_session.cpp
    src/serialization.cpp
    src/error.cpp
    src/redis_client.cpp
    src/websocket.cpp
    src/application.cpp
)
target_include_directories(servertech_chat PUBLIC include)
target_link_libraries(
    servertech_chat
    PUBLIC
    Boost::headers
    Boost::context
    Boost::json
    OpenSSL::Crypto
    OpenSSL::SSL
)
target_compile_definitions(
    servertech_chat
    PUBLIC
    BOOST_BEAST_SEPARATE_COMPILATION
    BOOST_ASIO_SEPARATE_COMPILATION
)
target_precompile_headers(
    servertech_chat
    PRIVATE
    include/pch.hpp
)

# The actual server executable
add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE servertech_chat)


# Testing
include(CTest)
if (BUILD_TESTING)
    add_subdirectory(test/integration)
endif()