
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/system/detail/error_code.hpp>

#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace http = boost::beast::http;
using boost::system::error_code;

namespace {

constexpr int num_iterations = 10000;
constexpr int num_clients = 10;

constexpr std::string_view
    msg = R"JSON({"type":"clientMessages","payload":{"roomId":"wasm","messages":[{"content":"hola"}]}})JSON";

void check(error_code ec)
{
    if (ec)
    {
        std::cerr << "Error: " << ec << ": " << ec.message() << std::endl;
        exit(1);
    }
}

struct condition_variable
{
    asio::steady_timer timer;
    int remaining;

    condition_variable(asio::any_io_executor ex, int remaining)
        : timer(std::move(ex), std::chrono::steady_clock::time_point::max()), remaining(remaining)
    {
    }

    void notify_one()
    {
        if (--remaining == 0)
            timer.expires_at(std::chrono::steady_clock::time_point::min());
    }
};

struct client
{
    websocket::stream<asio::ip::tcp::socket> ws;
    beast::flat_buffer buffer;
    condition_variable& cv;
    int send_iterations{num_iterations};
    int recv_iterations{num_iterations * num_clients};

    client(asio::any_io_executor ex, condition_variable& cv) : ws(std::move(ex)), cv(cv) {}

    void connect()
    {
        asio::co_spawn(
            ws.get_executor(),
            [this]() -> asio::awaitable<void> {
                // Connect
                co_await ws.next_layer().async_connect(
                    asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 8080)
                );

                // Write a login request
                http::request<http::string_body> login_req(http::verb::post, "/api/login", 11);
                login_req.set(http::field::content_type, "application/json");
                login_req.set(http::field::host, "localhost");
                login_req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
                login_req.body() = R"JSON({"email":"admin@gmail.com","password":"Useruser10!"})JSON";
                login_req.keep_alive(true);
                login_req.prepare_payload();
                co_await http::async_write(ws.next_layer(), login_req);

                // Read the response
                http::response<http::empty_body> login_res;
                co_await http::async_read(ws.next_layer(), buffer, login_res);
                if (login_res.result() != http::status::no_content)
                {
                    std::cerr << "Login request failed\n";
                    exit(1);
                }
                auto cookie = login_res.at(http::field::set_cookie);
                auto sid = cookie.substr(0, cookie.find(';'));  // sid=xxxxxxxx;yyyyyy

                // Set auth
                ws.set_option(websocket::stream_base::decorator([&](websocket::request_type& req) {
                    req.set(beast::http::field::cookie, sid);
                }));

                // Websocket handshake
                co_await ws.async_handshake("127.0.0.1:8080", "/api/ws");

                // Read the hello message
                co_await ws.async_read(buffer);

                // Done
                cv.notify_one();
            },
            [](std::exception_ptr exc) {
                if (exc)
                    std::rethrow_exception(exc);
            }
        );
    }

    void send()
    {
        ws.async_write(asio::buffer(msg), [this](error_code ec, std::size_t) {
            check(ec);
            if (--send_iterations == 0)
            {
                maybe_close();
                return;
            }
            send();
        });
    }

    void recv()
    {
        ws.async_read(buffer, [this](error_code ec, std::size_t) {
            check(ec);
            // if (recv_iterations < 10)
            //     std::cout << "Recv iterations: " << recv_iterations - 1 << std::endl;
            if (--recv_iterations == 0)
            {
                maybe_close();
                return;
            }
            recv();
        });
    }

    void maybe_close()
    {
        if (send_iterations == 0 && recv_iterations == 0)
        {
            ws.async_close(websocket::close_code::normal, asio::detached);
        }
    }
};

}  // namespace

int main()
{
    // The io_context is required for all I/O
    asio::io_context ctx;

    // Create and connect the clients
    condition_variable cv(ctx.get_executor(), num_clients);
    std::vector<client> clients;
    clients.reserve(num_clients);
    for (int i = 0; i < num_clients; ++i)
        clients.emplace_back(ctx.get_executor(), cv);
    for (auto& cli : clients)
        cli.connect();

    std::chrono::steady_clock::time_point tbegin, tend;

    cv.timer.async_wait([&](error_code) {
        std::cout << "Starting benchmark" << std::endl;
        // Benchmark starts here
        tbegin = std::chrono::steady_clock::now();

        // Launch the tasks
        for (auto& cli : clients)
        {
            cli.send();
            cli.recv();
        }
    });

    ctx.run();
    tend = std::chrono::steady_clock::now();

    std::cout << "Elapsed: " << std::chrono::duration_cast<std::chrono::milliseconds>(tend - tbegin).count()
              << "ms\n";
}