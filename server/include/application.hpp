//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_APPLICATION_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_APPLICATION_HPP

#include <memory>
#include <string>

#include "error.hpp"
#include "promise.hpp"

namespace chat {

struct application_config
{
    std::string doc_root;
    std::string ip;
    unsigned short port;
};

promise<error_code> run_application(const application_config& cfg);

}  // namespace chat

#endif
