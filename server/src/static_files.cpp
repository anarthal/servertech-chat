//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "static_files.hpp"

#include <boost/beast/http/verb.hpp>

#include <filesystem>

using namespace chat;
namespace beast = boost::beast;
namespace http = boost::beast::http;

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
// This is copied from Boost.Beast examples.
static std::string path_cat(std::string_view base, std::string_view path)
{
    constexpr char path_separator = '/';

    if (base.empty())
        return std::string(path);

    std::string result(base);

    if (result.back() == path_separator)
        result.resize(result.size() - 1);

    result.append(path.data(), path.size());

    return result;
}

response_builder::response_type chat::handle_static_file(request_context& ctx, shared_state& st)
{
    auto method = ctx.request_method();

    // Make sure we can handle the method
    if (method != http::verb::get && method != http::verb::head)
        return ctx.response().method_not_allowed();

    // Request path must be absolute and not contain ".."
    auto target = ctx.request_target();
    auto target_path = target.path();
    if (target.empty() || !target.is_path_absolute() || target_path.find("..") != beast::string_view::npos)
        return ctx.response().bad_request_text("Illegal request-target");

    // The special target / gets index.html
    if (target_path == "/")
        target_path = "/index.html";

    // Build the path to the requested file
    std::string path = path_cat(st.doc_root(), target_path);

    // If the filename doesn't have an extension, we infer html
    std::filesystem::path p(path);
    if (p.extension().empty())
        path.append(".html");

    // Send the file
    return ctx.response().file_response(path.c_str(), method == http::verb::head);
}