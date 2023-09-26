#
# Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

# We're using Boost.Test as testing framework
find_package(Boost REQUIRED COMPONENTS unit_test_framework)

# Test list
add_executable(
    unit_tests
    
    # main() definition
    entry_point.cpp 

    # Utility functions
    util/async_mutex.cpp
    util/base64.cpp
    util/email.cpp
    util/scrypt.cpp
    util/password_hash.cpp
    util/cookie.cpp

    # Services
    services/pubsub_service.cpp
    services/redis_serialization.cpp
    
    # API
    api/api_types.cpp
)

# Linking against servertech_chat allows us to use the functions under test
target_link_libraries(
    unit_tests
    PRIVATE
    servertech_chat
    Boost::unit_test_framework
)

# We only precompile Boost.Test headers
target_precompile_headers(
    unit_tests
    PRIVATE
    <boost/test/unit_test.hpp>
)

# Make ctest run the unit tests
add_test(
    NAME unit_tests
    COMMAND unit_tests
)