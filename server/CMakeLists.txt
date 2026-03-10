#
# Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.16.0)
project(servertech_chat)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Boost.Charconv is required by Boost.MySQL
find_package(Boost REQUIRED COMPONENTS headers json regex url charconv)

# OpenSSL is required by Boost.Redis
find_package(OpenSSL REQUIRED)

# ICU is required by Boost.Regex
find_package(ICU COMPONENTS data i18n uc REQUIRED)

# This library is consumed by the actual server and the tests
add_library(servertech_chat STATIC)
target_sources(servertech_chat
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS modules
        FILES

        # Utilities
        modules/util/base64.cppm
        modules/util/email.cppm
        modules/util/scrypt.cppm
        modules/util/password_hash.cppm
        modules/util/cookie.cppm
        modules/util/async_mutex.cppm
        modules/util/websocket.cppm

        # Services
        modules/services/redis_serialization.cppm
        modules/services/redis_client.cppm
        modules/services/mysql_client.cppm
        modules/services/session_store.cppm
        modules/services/cookie_auth_service.cppm
        modules/services/room_history_service.cppm
        modules/services/pubsub_service.cppm

        # API
        modules/api/api_types.cppm
        modules/api/auth.cppm
        modules/api/chat_websocket.cppm

        # Server
        modules/static_files.cppm
        modules/server.cppm
        modules/http_session.cppm
        modules/request_context.cppm
        modules/shared_state.cppm
        modules/business_types.cppm
        modules/timestamp.cppm
        modules/error.cppm
        modules/servertech_chat.cppm

    PRIVATE
        # Separate compilation
        src/separate-compilation/boost_asio.cpp
        src/separate-compilation/boost_beast.cpp
        src/separate-compilation/boost_mysql.cpp
        src/separate-compilation/boost_redis.cpp

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
        src/servertech_chat.cpp
)

target_compile_features(servertech_chat PUBLIC cxx_std_23)
set_target_properties(servertech_chat PROPERTIES CXX_MODULE_STD 1)

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


target_link_libraries(
    servertech_chat
    PUBLIC
    Boost::headers
    Boost::json
    Boost::regex
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

# The actual server executable
add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE servertech_chat)

# Testing
include(CTest)
if (BUILD_TESTING)
    add_subdirectory(test)
endif()
