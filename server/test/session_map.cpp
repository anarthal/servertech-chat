//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "session_map.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/test/unit_test.hpp>

#include <memory>
#include <set>

#include "websocket.hpp"
#include "websocket_session.hpp"

using namespace chat;

using session_set = std::set<websocket_session*>;

// Creates a dummy session object
static std::shared_ptr<websocket_session> make_session()
{
    return std::make_shared<websocket_session>(
        websocket(boost::asio::ip::tcp::socket(boost::asio::system_executor()), boost::beast::flat_buffer()),
        nullptr
    );
}

// Converts the result of session_map::get_sessions() into a set.
// Verifies that there are no duplicates
template <class ItPair>
static session_set to_set(ItPair itpair)
{
    auto [first, last] = itpair;
    session_set res;
    for (auto it = first; it != last; ++it)
    {
        auto insert_result = res.insert(it->session.get());
        BOOST_TEST(insert_result.second);  // Inserted OK - no duplicates
    }
    return res;
}

BOOST_AUTO_TEST_SUITE(session_map_)

BOOST_AUTO_TEST_CASE(get_sessions)
{
    // Data
    session_map map;
    auto sess1 = make_session();
    auto sess2 = make_session();
    const room sess1rooms[] = {{"r1"}, {"r2"}};
    const room sess2rooms[] = {{"r3"}, {"r1"}};

    // Add sessions
    map.add_session(sess1, sess1rooms);
    map.add_session(sess2, sess2rooms);

    // Get sessions for r1
    auto r1_sessions = to_set(map.get_sessions("r1"));
    BOOST_TEST((r1_sessions == session_set{sess1.get(), sess2.get()}));

    // Get sessions for r2
    auto r2_sessions = to_set(map.get_sessions("r2"));
    BOOST_TEST(r2_sessions == session_set{sess1.get()});

    // Get sessions for r3
    auto r3_sessions = to_set(map.get_sessions("r3"));
    BOOST_TEST(r3_sessions == session_set{sess2.get()});

    // Get sessions for r4
    auto r4_sessions = to_set(map.get_sessions("r4"));
    BOOST_TEST(r4_sessions == session_set{});
}

BOOST_AUTO_TEST_CASE(remove_session)
{
    // Data
    session_map map;
    auto sess1 = make_session();
    const room sess1rooms[] = {{"r1"}, {"r2"}};

    // Add the session
    map.add_session(sess1, sess1rooms);
    BOOST_TEST(to_set(map.get_sessions("r1")).size() == 1u);

    // Remove the session
    map.remove_session(*sess1);

    // No longer there
    BOOST_TEST(to_set(map.get_sessions("r1")).size() == 0u);
}

// Edge case: we don't crash if we try to remove a session that it's not there
BOOST_AUTO_TEST_CASE(remove_session_not_present)
{
    // Data
    session_map map;
    auto sess1 = make_session();

    // Remove the session
    BOOST_CHECK_NO_THROW(map.remove_session(*sess1));
}

BOOST_AUTO_TEST_SUITE_END()