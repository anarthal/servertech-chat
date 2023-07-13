# copy

cmake_minimum_required(VERSION 3.16.0)
project(servertech_chat)

find_package(Boost REQUIRED COMPONENTS headers)
find_package(OpenSSL REQUIRED)

add_executable(
    main
    src/boost_asio.cpp
    src/boost_beast.cpp
    src/http_session.cpp
    src/listener.cpp
    src/shared_state.cpp
    src/websocket_session.cpp
    src/main.cpp
)
target_link_libraries(
    main
    PRIVATE
    Boost::headers
    OpenSSL::Crypto
    OpenSSL::SSL
)
target_compile_definitions(
    main
    PRIVATE
    BOOST_BEAST_SEPARATE_COMPILATION
    BOOST_ASIO_SEPARATE_COMPILATION
)
target_precompile_headers(
    main
    PRIVATE
    src/pch.hpp
)