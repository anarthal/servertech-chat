// copyright
#ifndef BOOST_MYSQL_
#define BOOST_MYSQL_

#include <boost/core/span.hpp>

#include <memory>
#include <thread>
#include <vector>

#include "application.hpp"

namespace chat {
namespace test {

class websocket_client
{
public:
    struct impl;

    websocket_client(impl*) noexcept;
    websocket_client(const websocket_client&) = delete;
    websocket_client(websocket_client&&) noexcept;
    websocket_client& operator=(const websocket_client&) = delete;
    websocket_client& operator=(websocket_client&&) noexcept;
    ~websocket_client();

    std::vector<unsigned char> read();
    void write(boost::span<const unsigned char> buffer);

private:
    std::unique_ptr<impl> impl_;
};

class server_runner
{
    application app;
    std::thread runner;

public:
    server_runner();
    server_runner(const server_runner&) = delete;
    server_runner(server_runner&&) = delete;
    server_runner& operator=(const server_runner&) = delete;
    server_runner& operator=(server_runner&&) = delete;
    ~server_runner();

    websocket_client connect_websocket();
};

}  // namespace test
}  // namespace chat

#endif
