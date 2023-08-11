#
# Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

find_package(Boost REQUIRED COMPONENTS unit_test_framework)

add_executable(
    unit_tests
    entry_point.cpp
    async_mutex.cpp
    redis_serialization.cpp
    api_serialization.cpp
    session_map.cpp
)

target_link_libraries(
    unit_tests
    PRIVATE
    servertech_chat
    Boost::unit_test_framework
)

target_precompile_headers(
    unit_tests
    PRIVATE
    <boost/test/unit_test.hpp>
)

add_test(
    NAME unit_tests
    COMMAND unit_tests
)