#!/bin/bash
#
# Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

FNAME=src/pch.hpp

cat > $FNAME <<- EOM
//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECH_PCH
#define SERVERTECH_PCH

EOM

grep -rh '#include <' src/server/  | sort | uniq >> $FNAME

cat >> $FNAME <<- EOM

#endif

EOM