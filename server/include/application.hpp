// copyright
#ifndef BOOST_MYSQL_
#define BOOST_MYSQL_

#include <memory>
#include <string>

namespace chat {

struct application_config
{
    std::string doc_root;
    std::string ip;
    unsigned short port;
};

class application
{
    struct impl;
    std::unique_ptr<impl> impl_;

public:
    application(application_config config);
    application(const application&) = delete;
    application(application&&) noexcept;
    application& operator=(const application&) = delete;
    application& operator=(application&&) noexcept;
    ~application();

    // Launches the application and runs until explicitly stopped by stop().
    // If with_signal_handlers, registers signal handlers that call stop()
    void run_until_completion(bool with_signal_handlers);

    void stop();
};

}  // namespace chat

#endif
