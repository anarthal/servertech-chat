//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_UTIL_COOKIE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_UTIL_COOKIE_HPP

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace chat {

// The SameSite cookie attribute
enum class same_site_t
{
    strict,
    lax,  // this is the default
    none,
};

// A builder for the Set-Cookie header
class set_cookie_builder
{
    std::string_view name_;
    std::string_view value_;
    bool http_only_{false};
    std::optional<std::chrono::seconds> max_age_{};
    same_site_t same_site_{same_site_t::lax};
    bool secure_{false};

public:
    // Constructs a builder for a (cookie-name, cookie-value) pair.
    // name should be a valid HTTP token; value should be a valid cookie
    // value (see cookie.cpp for details). Otherwise, an exception is thrown.
    set_cookie_builder(std::string_view name, std::string_view value);

    // Sets the HttpOnly attribute
    set_cookie_builder& http_only(bool value) noexcept
    {
        http_only_ = value;
        return *this;
    }

    // Sets the Max-Age attribute
    set_cookie_builder& max_age(std::chrono::seconds val) noexcept
    {
        max_age_ = val;
        return *this;
    }

    // Sets the SameSite attribute
    set_cookie_builder& same_site(same_site_t val) noexcept
    {
        same_site_ = val;
        return *this;
    }

    // Sets the Secure attribute
    set_cookie_builder& secure(bool val) noexcept
    {
        secure_ = val;
        return *this;
    }

    // Builds the Set-Cookie header
    std::string build_header() const;
};

// A non-owning (cookie-name, cookie-value) pair
struct cookie_pair
{
    std::string_view name;
    std::string_view value;
};
inline bool operator==(const cookie_pair& lhs, const cookie_pair& rhs) noexcept
{
    return lhs.name == rhs.name && lhs.value == rhs.value;
}
inline bool operator!=(const cookie_pair& lhs, const cookie_pair& rhs) noexcept { return !(lhs == rhs); }

// A zero-copy parser for the Cookie header (Beast-style).
// Used to parse incoming cookies.
//
// If a parsing error is encountered while iterating the string,
// the behavior of the container will be as if a string containing
// only characters up to but excluding the first invalid character
// was used to construct the list.
class cookie_list
{
    std::string_view header_;

public:
    // Cookie name, cookie value
    using value_type = cookie_pair;

    // Iterator
    class const_iterator
    {
    public:
        using value_type = cookie_list::value_type;
        using pointer = const value_type*;
        using reference = const value_type&;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;

        const_iterator() = default;

        bool operator==(const const_iterator& rhs) const noexcept
        {
            return val_ == rhs.val_ && next_ == rhs.next_ && last_ == rhs.last_;
        }

        bool operator!=(const const_iterator& rhs) const noexcept { return !(*this == rhs); }

        reference operator*() const noexcept { return val_; }

        pointer operator->() const noexcept { return &val_; }

        const_iterator& operator++() noexcept
        {
            increment(false);
            return *this;
        }

        const_iterator operator++(int) noexcept
        {
            auto temp = *this;
            ++(*this);
            return temp;
        }

    private:
        friend class cookie_list;

        value_type val_;
        const char* next_{};
        const char* last_{};

        const_iterator(const char* first, const char* last) noexcept : next_(first), last_(last)
        {
            // Parse the first cookie. This is called from begin()
            increment(true);
        }

        void increment(bool is_first) noexcept;
        void reset() noexcept { *this = const_iterator(); }
    };

    // Constructs an empty cookie list
    cookie_list() = default;

    // Constructs a cookie list from a header string. No copies of the header
    // are performed.
    explicit cookie_list(std::string_view header) noexcept;

    // Parses the first cookie and returns an iteator to it
    const_iterator begin() const noexcept
    {
        return header_.empty() ? const_iterator()
                               : const_iterator(header_.data(), header_.data() + header_.size());
    }

    // One-past-the-end sentinel iterator
    const_iterator end() const noexcept { return const_iterator(); }
};

}  // namespace chat

#endif
