//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "util/cookie.hpp"

#include <sstream>
#include <stdexcept>
#include <string_view>

#include "error.hpp"

using namespace chat;

// Copied from Boost.Beast. Returns whether a character is valid in the context
// of a HTTP token (RFC2616/RFC7230). Cookie names must be valid HTTP tokens.
static bool is_token_char(char c) noexcept
{
    /*
        tchar = "!" | "#" | "$" | "%" | "&" |
                "'" | "*" | "+" | "-" | "." |
                "^" | "_" | "`" | "|" | "~" |
                DIGIT | ALPHA
    */
    static char constexpr tab[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 16
        0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0,  // 32
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  // 48
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 64
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1,  // 80
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 96
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0,  // 112
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 128
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 144
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 160
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 176
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 192
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 208
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 224
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   // 240
    };
    static_assert(sizeof(tab) == 256u);
    return tab[static_cast<unsigned char>(c)];
}

// Returns whether a character is valid in the context of a cookie value.
// See https://datatracker.ietf.org/doc/html/rfc6265#section-4.1.1
static bool is_cookie_value_char(char c) noexcept
{
    //  cookie-octet      = %x21 / %x23-2B / %x2D-3A / %x3C-5B / %x5D-7E
    //                    ; US-ASCII characters excluding CTLs,
    //                    ; whitespace DQUOTE, comma, semicolon,
    //                    ; and backslash
    static char constexpr tab[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 16
        0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,  // 32
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1,  // 48
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 64
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,  // 80
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 96
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,  // 112
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 128
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 144
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 160
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 176
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 192
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 208
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 224
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   // 240
    };
    static_assert(sizeof(tab) == 256u);
    return tab[static_cast<unsigned char>(c)];
}

// Skips optional whitespace
static const char* skip_ows(const char* it, const char* end) noexcept
{
    while (it != end && (*it == ' ' || *it == '\t'))
        ++it;
    return it;
}

// Skips a token
static const char* skip_token(const char* it, const char* last) noexcept
{
    while (it != last && is_token_char(*it))
        ++it;
    return it;
}

// Skips a cookie value
static const char* skip_cookie_value(const char* it, const char* last) noexcept
{
    while (it != last && is_cookie_value_char(*it))
        ++it;
    return it;
}

static bool is_valid_token(std::string_view value) noexcept
{
    if (value.empty())
        return false;
    for (char c : value)
    {
        if (!is_token_char(c))
            return false;
    }
    return true;
}

static bool is_valid_cookie_value(std::string_view value) noexcept
{
    if (value.empty())
        return false;
    for (char c : value)
    {
        if (!is_cookie_value_char(c))
            return false;
    }
    return true;
}

static std::string_view trim_ows(std::string_view from) noexcept
{
    const char* first = from.data();
    const char* last = from.data() + from.size();
    first = skip_ows(first, last);
    return std::string_view(first, last - first);
}

set_cookie_builder::set_cookie_builder(std::string_view name, std::string_view value)
    : name_(name), value_(value)
{
    if (!is_valid_token(name_))
        throw std::invalid_argument("Invalid cookie name");
    if (!is_valid_cookie_value(value_))
        throw std::invalid_argument("Invalid cookie value");
}

std::string set_cookie_builder::build_header() const
{
    std::ostringstream oss;
    oss << name_ << '=' << value_;
    if (http_only_)
        oss << "; HttpOnly";
    if (max_age_)
        oss << "; Max-Age=" << max_age_->count();
    if (same_site_ != same_site_t::lax)
        oss << "; SameSite=" << (same_site_ == same_site_t::none ? "None" : "Strict");
    if (secure_)
        oss << "; Secure";
    return oss.str();
}

// Grammar for the Cookie header
// https://datatracker.ietf.org/doc/html/rfc6265
//
//      cookie-header     = "Cookie:" OWS cookie-string OWS
//      cookie-string     = cookie-pair *( ";" SP cookie-pair )
//      cookie-pair       = cookie-name "=" cookie-value
//      cookie-name       = token
//      cookie-value      = *cookie-octet / ( DQUOTE *cookie-octet DQUOTE )
//      cookie-octet      = %x21 / %x23-2B / %x2D-3A / %x3C-5B / %x5D-7E
//                        ; US-ASCII characters excluding CTLs,
//                        ; whitespace DQUOTE, comma, semicolon,
//                        ; and backslash

// Trim the initial OWS. The final OWS can be left there, since when no more cookies
// are found, increment() sets the iterator to end()
cookie_list::cookie_list(std::string_view header) noexcept : header_(trim_ows(header)) {}

void cookie_list::const_iterator::increment(bool is_first) noexcept
{
    // Check that this is not a sentinel iterator
    assert(next_ != nullptr);
    assert(last_ != nullptr);

    const char* current = next_;
    const char* const last = last_;

    // If we're parsing subsequent cookies, skip a semicolon and a space
    if (!is_first)
    {
        if (current == last || *current++ != ';')
            return reset();
        if (current == last || *current++ != ' ')
            return reset();
    }

    // Cookie name. Empty cookie names are not valid
    auto name_first = current;
    current = skip_token(current, last);
    std::string_view name(name_first, current - name_first);
    if (name.empty())
        return reset();

    // Equal sign
    if (current == last || *current++ != '=')
        return reset();

    // Cookie value. Note that quotes are part of the value
    auto value_first = current;
    bool is_quoted = false;
    if (current != last && *current == '"')
    {
        ++current;
        is_quoted = true;
    }
    current = skip_cookie_value(current, last);
    if (is_quoted)
    {
        if (current == last || *current != '"')
            return reset();
        ++current;
    }
    std::string_view value(value_first, current - value_first);

    // Done parsing
    val_.name = name;
    val_.value = value;
    next_ = current;
    last_ = last;
}
