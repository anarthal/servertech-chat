//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "request_context.hpp"

#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/url/parse.hpp>

#include <string_view>

#include "api/api_types.hpp"
#include "error.hpp"

using namespace chat;
namespace http = boost::beast::http;
namespace beast = boost::beast;

// Intentionally don't provide exact version
static constexpr std::string_view server_header = "beast";

static std::string_view mime_type(std::string_view path)
{
    using beast::iequals;
    auto const ext = [&path] {
        auto const pos = path.rfind(".");
        if (pos == std::string_view::npos)
            return std::string_view{};
        return path.substr(pos);
    }();
    if (iequals(ext, ".htm"))
        return "text/html";
    if (iequals(ext, ".html"))
        return "text/html";
    if (iequals(ext, ".php"))
        return "text/html";
    if (iequals(ext, ".css"))
        return "text/css";
    if (iequals(ext, ".txt"))
        return "text/plain";
    if (iequals(ext, ".js"))
        return "application/javascript";
    if (iequals(ext, ".json"))
        return "application/json";
    if (iequals(ext, ".xml"))
        return "application/xml";
    if (iequals(ext, ".swf"))
        return "application/x-shockwave-flash";
    if (iequals(ext, ".flv"))
        return "video/x-flv";
    if (iequals(ext, ".png"))
        return "image/png";
    if (iequals(ext, ".jpe"))
        return "image/jpeg";
    if (iequals(ext, ".jpeg"))
        return "image/jpeg";
    if (iequals(ext, ".jpg"))
        return "image/jpeg";
    if (iequals(ext, ".gif"))
        return "image/gif";
    if (iequals(ext, ".bmp"))
        return "image/bmp";
    if (iequals(ext, ".ico"))
        return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff"))
        return "image/tiff";
    if (iequals(ext, ".tif"))
        return "image/tiff";
    if (iequals(ext, ".svg"))
        return "image/svg+xml";
    if (iequals(ext, ".svgz"))
        return "image/svg+xml";
    return "application/text";
}

response_builder::response_builder(unsigned version, bool keep_alive) : keep_alive_(keep_alive)
{
    header_.version(version);
    header_.set(http::field::server, server_header);
}

response_builder::response_type response_builder::file_response(const char* path, bool is_head)
{
    // Attempt to open the file
    error_code ec;
    http::file_body::value_type body;
    body.open(path, beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if (ec == boost::system::errc::no_such_file_or_directory)
        return not_found_text();

    // Handle an unknown error
    if (ec)
        return internal_server_error(ec, "Opening file");

    // Cache the size since we need it after the move
    const auto size = body.size();
    set_content_type(mime_type(path));

    if (is_head)
    {
        // Respond to HEAD request
        auto res = build_response<http::empty_body>();
        res.content_length(size);
        return res;
    }
    else
    {
        // Respond to GET request
        auto res = build_response<http::file_body>(std::move(body));
        res.content_length(size);
        return res;
    }
}

response_builder::response_type response_builder::empty_response()
{
    header_.result(http::status::no_content);
    auto res = build_response<http::empty_body>();
    res.prepare_payload();
    return res;
}

response_builder::response_type response_builder::json_response_impl(std::string serialized_json)
{
    set_content_type("application/json");
    auto res = build_response<http::string_body>(std::move(serialized_json));
    res.prepare_payload();
    return res;
}

response_builder::response_type response_builder::plaintext_response(
    boost::beast::http::status status,
    std::string content
)
{
    header_.result(status);
    set_content_type("text/plain");
    auto res = build_response<http::string_body>(std::move(content));
    res.prepare_payload();
    return res;
}

response_builder::response_type response_builder::json_error(
    boost::beast::http::status status,
    api_error_id error_id,
    std::string_view error_message
)
{
    header_.result(status);
    return json_response(api_error{error_id, error_message});
}

response_builder::response_type response_builder::internal_server_error(error_code ec, std::string_view what)
{
    // Log the error
    log_error(ec, "Returning internal server error", what);

    // Intentionally don't expose any error information
    return plaintext_response(
        boost::beast::http::status::internal_server_error,
        "An unexpected server error occurred"
    );
}

error_code request_context::parse_request_target()
{
    auto url_result = boost::urls::parse_origin_form(request_.target());
    if (url_result.has_error())
        return url_result.error();
    target_ = url_result.value();
    return error_code();
}

bool request_context::is_json_content_type() const
{
    // Validate content-type
    auto it = request_.find(boost::beast::http::field::content_type);
    return it != request_.end() && it->value() == "application/json";
}