#!/bin/bash
#
# Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

grep -rh '#include <' src/server/  | sort | uniq > src/pch.hpp