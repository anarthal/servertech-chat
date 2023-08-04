
find_package(Boost REQUIRED COMPONENTS unit_test_framework)

add_executable(
    integ_tests
    entry_point.cpp
    websocket.cpp
)
target_link_libraries(
    integ_tests
    PRIVATE
    Boost::unit_test_framework
)
target_precompile_headers(
    integ_tests
    PRIVATE
    <boost/test/unit_test.hpp>
)
