//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_SCRYPT_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_SCRYPT_HPP

#include <boost/system/result.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Utility functions to hash and check passwords using the
// scrypt algorithm (https://en.wikipedia.org/wiki/Scrypt)
// and the PHC format (https://github.com/P-H-C/phc-string-format/blob/master/phc-sf-spec.md)

namespace chat {

// Constants and defaults. These are based on node.js defaults
inline constexpr std::size_t salt_size = 32;
inline constexpr std::uint64_t default_ln = 14;
inline constexpr std::uint64_t default_r = 8;
inline constexpr std::uint64_t default_p = 1;
inline constexpr std::size_t hash_size = 32;

// Algorithm parameters, user-independent
struct scrypt_params
{
    std::uint64_t ln{default_ln};
    std::uint64_t r{default_r};
    std::uint64_t p{default_p};
};

// The result of parsing a scypt PHC string. Note that salt and hash
// can have a different size than the defaults listed here. Allowing this
// allows us to change the defaults without breaking existing data
struct scrypt_data
{
    scrypt_params params;
    std::vector<unsigned char> salt;
    std::vector<unsigned char> hash;
};

// Parses a PHC scrypt string
boost::system::result<scrypt_data> scrypt_phc_parse(std::string_view from);

// Serializes the given params, salt and hash to a PHC string
std::string scrypt_phc_serialize(
    scrypt_params params,
    std::span<const unsigned char, salt_size> salt,
    std::span<const unsigned char, hash_size> hash
);

// Hashes the given password with the given salt and params
std::array<unsigned char, hash_size> scrypt_generate_hash(
    std::string_view passwd,
    scrypt_params params,
    std::span<const unsigned char> salt
);

// Compares two blobs, in a way that prevents timing attacks
bool time_safe_equals(std::span<const unsigned char> s1, std::span<const unsigned char> s2) noexcept;

}  // namespace chat

#endif
