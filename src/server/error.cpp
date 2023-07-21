// copy

#include "error.hpp"

static const char* to_string(chat::errc v) noexcept
{
    switch (v)
    {
    case chat::errc::redis_parse_error: return "redis_parse_error";
    default: return "<unknown chat error>";
    }
}

namespace chat {

class chat_category final : public boost::system::error_category
{
public:
    const char* name() const noexcept final override { return "chat"; }
    std::string message(int ev) const final override { return to_string(static_cast<chat::errc>(ev)); }
};

}  // namespace chat

static chat::chat_category cat;

const boost::system::error_category& chat::get_chat_category() noexcept { return cat; }