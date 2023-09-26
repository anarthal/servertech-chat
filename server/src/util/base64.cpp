//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/base64.hpp"

#include <boost/core/ignore_unused.hpp>

#include <cstddef>
#include <string>
#include <string_view>

#include "error.hpp"

// This code has been copied and adapted from Boost.Beast implementation, since the
// interface is not public.

using namespace chat;

// Number of bytes of expected padding for a given input data length
static std::size_t get_padding(std::size_t data_length) noexcept
{
    switch (data_length % 3)
    {
    case 1: return 2;
    case 2: return 1;
    default: return 0;
    }
}

// Returns the max number of bytes needed to encode a string
static constexpr std::size_t encoded_size(std::size_t n) noexcept { return 4 * ((n + 2) / 3); }

// Returns the max number of bytes needed to decode a base64 string
static constexpr std::size_t decoded_size(std::size_t n) noexcept { return (n + 4) / 4 * 3; }

// Forward table
static constexpr char alphabet[] = {
    "ABCDEFGHIJKLMNOP"
    "QRSTUVWXYZabcdef"
    "ghijklmnopqrstuv"
    "wxyz0123456789+/"};

// Inverse table
static constexpr signed char inverse_tab[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  //   0-15
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  //  16-31
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,  //  32-47
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,  //  48-63
    -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,  //  64-79
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,  //  80-95
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,  //  96-111
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,  // 112-127
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 128-143
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 144-159
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 160-175
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 176-191
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 192-207
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 208-223
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 224-239
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1   // 240-255
};

// Encodes src and stores it in dest. dest must point to encoded_size(src.size()) bytes.
// Returns the number of written characters
static std::size_t encode(char* dest, boost::span<const unsigned char> src, bool with_padding) noexcept
{
    char* out = dest;
    const unsigned char* in = src.data();
    std::size_t len = src.size();
    const char* tab = alphabet;

    for (auto n = len / 3; n--;)
    {
        *out++ = tab[(in[0] & 0xfc) >> 2];
        *out++ = tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
        *out++ = tab[((in[2] & 0xc0) >> 6) + ((in[1] & 0x0f) << 2)];
        *out++ = tab[in[2] & 0x3f];
        in += 3;
    }

    switch (len % 3)
    {
    case 2:
        *out++ = tab[(in[0] & 0xfc) >> 2];
        *out++ = tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
        *out++ = tab[(in[1] & 0x0f) << 2];
        if (with_padding)
            *out++ = '=';
        break;

    case 1:
        *out++ = tab[(in[0] & 0xfc) >> 2];
        *out++ = tab[((in[0] & 0x03) << 4)];
        if (with_padding)
        {
            *out++ = '=';
            *out++ = '=';
        }
        break;

    case 0: break;
    }

    return out - dest;
}

// Decodes src and stores it in dest. dest must be decoded_size(len) bytes, at least.
// Retuns the number of actual bytes populated in dest, and a pointer to the
// first unparsed input character.
static result<std::pair<std::size_t, const char*>> decode(
    std::string_view from,
    unsigned char* dest,
    bool with_padding
)
{
    const char* src = from.data();
    const char* last = src + from.size();
    auto* out = dest;
    unsigned char c3[3], c4[4] = {0, 0, 0, 0};
    int i = 0;
    int j = 0;

    const signed char* inverse = inverse_tab;

    while (src != last && *src != '=')
    {
        const auto v = inverse[static_cast<std::size_t>(*src)];
        if (v == -1)
            CHAT_RETURN_ERROR(errc::invalid_base64)
        ++src;
        c4[i] = v;
        if (++i == 4)
        {
            c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
            c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
            c3[2] = ((c4[2] & 0x3) << 6) + c4[3];

            for (i = 0; i < 3; i++)
                *out++ = c3[i];
            i = 0;
        }
    }

    if (i)
    {
        c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
        c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
        c3[2] = ((c4[2] & 0x3) << 6) + c4[3];

        for (j = 0; j < i - 1; j++)
            *out++ = c3[j];
    }

    auto out_len = out - dest;

    // Check padding
    if (with_padding)
    {
        auto expected_padding = get_padding(out_len);
        if (last - src < expected_padding)
            CHAT_RETURN_ERROR(errc::invalid_base64)
        for (const char* pad_last = src + expected_padding; src != pad_last; ++src)
        {
            if (*src != '=')
                CHAT_RETURN_ERROR(errc::invalid_base64)
        }
    }

    return std::pair<std::size_t, const char*>{out_len, src};
}

std::string chat::base64_encode(boost::span<const unsigned char> input, bool with_padding)
{
    // Allocate space
    std::size_t max_size = encoded_size(input.size());
    std::string res(max_size, '\0');

    // Decode
    auto actual_size = encode(res.data(), input, with_padding);

    // Remove excess space
    assert(actual_size <= max_size);
    res.resize(actual_size);

    return res;
}

result<std::vector<unsigned char>> chat::base64_decode(std::string_view input, bool with_padding)
{
    // Allocate space
    std::size_t max_out_size = decoded_size(input.size());
    std::vector<unsigned char> res(max_out_size, 0);

    // Decode
    auto decode_result = decode(input, res.data(), with_padding);
    if (decode_result.has_error())
        return decode_result.error();
    auto [out_size, last] = decode_result.value();

    // Check that we consumed all the string
    if (last != input.data() + input.size())
        CHAT_RETURN_ERROR(errc::invalid_base64)

    // Remove any extra space
    res.resize(out_size);

    return res;
}