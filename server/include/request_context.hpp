//
// Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_REQUEST_CONTEXT_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_REQUEST_CONTEXT_HPP

#include <boost/beast/http/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/url_view.hpp>

#include <cassert>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "api/api_types.hpp"
#include "error.hpp"

// Contains a request_context class, which encapsulates a Boost.Beast HTTP request
// and provides an easy way to build HTTP responses.

namespace chat {

// Provides an easy way to build HTTP responses. This class is instantiated by
// the request_context.
// All the functions retunrning an actual response can be called at most once,
// since they will move internal state.
class response_builder
{
public:
    // The type of the HTTP responses. We use the type-erased message_generator class to allow
    // bodies of different types (e.g. string, file...) with a uniform interface.
    using response_type = boost::beast::http::message_generator;

    // Sets a cookie in the response. set_cookie_header must be the full value
    // of the Set-Cookie header to issue, as returned by cookie_builder::build_header()
    response_builder& set_cookie(std::string_view set_cookie_header)
    {
        assert(!used_);
        header_.set(boost::beast::http::field::set_cookie, set_cookie_header);
        return *this;
    }

    // Sets the content-type of the response
    response_builder& set_content_type(std::string_view value)
    {
        assert(!used_);
        header_.set(boost::beast::http::field::content_type, value);
        return *this;
    }

    // Sends a file as response. If only_headers is true, it will only send
    // the headers, and not the body (useful for HEAD requests).
    // Sends a 404 reponse if the file doesn't exist.
    // path must be absolute. No sanitization is performed on path - care must be
    // taken to prevent directory traversal attacks.
    response_type file_response(const char* path, bool only_headers = false);

    // Sends a 200 response with a JSON body. The type T must have a to_json() const
    // member function returning a string that performs the JSON serialization.
    template <class T>
    response_type json_response(const T& value)
    {
        return json_response_impl(value.to_json());
    }

    // Returns an empty response (204).
    response_type empty_response();

    // Returns a "method not allowed" response with a simple plaintext body.
    response_type method_not_allowed()
    {
        return plaintext_response(boost::beast::http::status::method_not_allowed, "Method not allowed");
    }

    // Returns a "bad request" response with a simple plaintext body.
    response_type bad_request_text(std::string why)
    {
        return plaintext_response(boost::beast::http::status::bad_request, std::move(why));
    }

    // Returns a "not found" response with a simple plaintext body.
    response_type not_found_text()
    {
        return plaintext_response(boost::beast::http::status::not_found, "Not found");
    }

    // Returns an error response, with a JSON body describing what happened.
    // See the api_error struct for the JSON schema of this response.
    // Used by the API, to communicate errors that are likely  to happen during normal operation
    // and should be processed by the client, like login failures.
    response_type json_error(
        boost::beast::http::status status,
        api_error_id error_id,
        std::string_view error_message
    );

    // Returns a bad request error response, with the JSON body described above.
    response_type bad_request_json(api_error_id error_id, std::string_view error_message)
    {
        return json_error(boost::beast::http::status::bad_request, error_id, error_message);
    }

    // Returns a bad request error response, with the JSON body described above,
    // and a generic error ID.
    // Used for validation errors that don't need to be handled by the client.
    response_type bad_request_json(std::string_view error_message)
    {
        return bad_request_json(api_error_id::bad_request, error_message);
    }

    // Returns an internal server error response. Error information is logged
    // but not sent in the response.
    response_type internal_server_error(error_code ec, std::string_view what);
    response_type internal_server_error(const error_with_message& err)
    {
        return internal_server_error(err.ec, err.msg);
    }

private:
    using header_type = boost::beast::http::response_header<boost::beast::http::fields>;

    bool keep_alive_;
    header_type header_;
    bool used_{};

    response_builder(unsigned version, bool keep_alive);
    response_type plaintext_response(boost::beast::http::status status, std::string content);
    response_type json_response_impl(std::string serialized_json);

    header_type move_header()
    {
        assert(!used_);
        used_ = true;
        return std::move(header_);
    }

    template <class Body, class... Args>
    boost::beast::http::response<Body> build_response(Args&&... args)
    {
        boost::beast::http::response<Body> res{move_header(), std::forward<Args>(args)...};
        res.keep_alive(keep_alive_);
        return res;
    }

    friend class request_context;
};

// Encapsulates a Boost.Beast request and provides an easy way to build responses.
// Intended to be passed to the HTTP API handler functions.
class request_context
{
public:
    // The Boost.Beast request type
    using request_type = boost::beast::http::request<boost::beast::http::string_body>;

    // Constructor
    request_context(request_type&& req)
        : request_(std::move(req)), response_(request_.version(), request_.keep_alive())
    {
    }

    // Parses the HTTP request target line into a URL. Returns an error_code on
    // failure. This should be called prior to invoking any API handler functions.
    error_code parse_request_target();

    // Returns the request target as a URL. parse_request_target must have been
    // called and succeeded.
    const boost::urls::url_view& request_target() const
    {
        assert(target_.has_value());
        return *target_;
    }

    // Returns the HTTP method of the request.
    boost::beast::http::verb request_method() const noexcept { return request_.method(); }

    // Attempts to parse the request body as JSON, and converts the result
    // to type T. T must have a static member function with signature
    // result<T> from_json(std::string_view).
    // The request content-type is validated before attempting the parse.
    template <class T>
    result<T> parse_json_body() const
    {
        // Validate content-type
        if (!is_json_content_type())
            CHAT_RETURN_ERROR(errc::invalid_content_type)

        // Parse the json
        return T::from_json(request_.body());
    }

    // Returns a response_builder object
    response_builder& response() noexcept { return response_; }

private:
    request_type request_;
    response_builder response_;
    std::optional<boost::urls::url_view> target_;

    bool is_json_content_type() const;
};

}  // namespace chat

#endif
