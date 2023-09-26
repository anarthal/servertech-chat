//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/password_hash.hpp"

#include <openssl/rand.h>
#include <stdexcept>

#include "util/scrypt.hpp"

using namespace chat;

std::string chat::hash_password(std::string_view passwd)
{
    // Default params
    constexpr scrypt_params params{};

    // Generate the salt. We use the private random generator because these hashes
    // are never exposed to the user
    std::array<unsigned char, salt_size> salt{};
    int ec = RAND_priv_bytes(salt.data(), salt.size());
    if (ec <= 0)
        throw std::runtime_error("Hashing password: RAND_priv_bytes");

    // Generate the hash
    auto hash = scrypt_generate_hash(passwd, params, salt);

    // Format all params into a P-H-C string
    return scrypt_phc_serialize(params, salt, hash);
}

bool chat::verify_password(std::string_view passwd, std::string_view hashed_passwd)
{
    // Deserialize the hashed password
    auto data_result = scrypt_phc_parse(hashed_passwd);
    if (data_result.has_error())
    {
        log_error(data_result.error(), "verify_password: malformed hash");
        return false;
    }
    const auto& stored_data = data_result.value();

    // Hash the incoming password with the params we used
    auto incoming_hash = scrypt_generate_hash(passwd, stored_data.params, stored_data.salt);

    // Compare passwords
    return time_safe_equals(stored_data.hash, incoming_hash);
}