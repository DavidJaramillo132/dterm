cmake_minimum_required(VERSION 3.20)
project(dterm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

include_directories(protocol)

add_executable(dterm
        server/main.cpp
        server/terminal.cpp
        server/server.cpp
        protocol/protocol.cpp
        protocol/auth.cpp
)
target_link_libraries(dterm PRIVATE OpenSSL::Crypto)

add_executable(dterm-client
        client/main.cpp
        client/raw_mode.cpp
        protocol/protocol.cpp
        protocol/auth.cpp
)
target_link_libraries(dterm-client PRIVATE OpenSSL::Crypto)

add_executable(protocol-test
        tests/protocol_test.cpp
        protocol/protocol.cpp
)
