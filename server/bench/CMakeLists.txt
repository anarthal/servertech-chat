#
# Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.16.0)
project(bench)

find_package(boost_headers REQUIRED)


add_executable(bench bench.cpp)
target_link_libraries(bench PRIVATE Boost::headers)