#
# Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.16.0)
project(servertech_chat)

# Boost.Charconv is required by Boost.MySQL
find_package(Boost REQUIRED COMPONENTS headers json url charconv)

# OpenSSL is required by Boost.Redis
find_package(OpenSSL REQUIRED)

# ICU is required by Boost.Regex
find_package(ICU COMPONENTS data i18n uc REQUIRED)

# This library is consumed by the actual server and the tests
add_library(
    servertech_chat
    STATIC

    # Header-only libraries with optional separate-source mode
    src/separate-compilation/boost_asio.cpp
    src/separate-compilation/boost_beast.cpp
    src/separate-compilation/boost_redis.cpp
    src/separate-compilation/boost_mysql.cpp

    # Utilities
    src/util/base64.cpp
    src/util/email.cpp
    src/util/scrypt.cpp
    src/util/password_hash.cpp
    src/util/cookie.cpp
    src/util/websocket.cpp

    # Services
    src/services/redis_serialization.cpp
    src/services/redis_client.cpp
    src/services/mysql_client.cpp
    src/services/session_store.cpp
    src/services/cookie_auth_service.cpp
    src/services/room_history_service.cpp
    src/services/pubsub_service.cpp

    # API
    src/api/api_types.cpp
    src/api/auth.cpp
    src/api/chat_websocket.cpp

    # Server
    src/static_files.cpp
    src/server.cpp
    src/http_session.cpp
    src/request_context.cpp
    src/shared_state.cpp
    src/error.cpp
)

target_link_libraries(
    servertech_chat
    PUBLIC
    Boost::headers
    Boost::json
    Boost::url
    Boost::charconv
    OpenSSL::Crypto
    OpenSSL::SSL
    ICU::data
    ICU::i18n
    ICU::uc
)

target_include_directories(
    servertech_chat
    PUBLIC
    include
)

# Coroutines require C++20
target_compile_features(
    servertech_chat
    PUBLIC
    cxx_std_20
)

# These are required because we're building Boost.Asio, Boost.Beast and Boost.MySQL
# in separate-compilation mode (see boost_asio.cpp, boost_beast.cpp, boost_mysql.cpp).
# Boost.Redis doesn't need such definition, even when building it like we're doing.
target_compile_definitions(
    servertech_chat
    PUBLIC
    BOOST_BEAST_SEPARATE_COMPILATION
    BOOST_ASIO_SEPARATE_COMPILATION
    BOOST_MYSQL_SEPARATE_COMPILATION
)

# Precompiled headers, to reduce build times
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
    add_subdirectory(test)
endif()
