cmake_minimum_required(VERSION 3.20)
project(dterm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

include_directories(protocol)

add_executable(dterm
        server/main.cpp
        server/handshake.cpp
        server/session.cpp
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

enable_testing()

add_test(NAME protocol COMMAND protocol-test)

add_test(NAME integration
        COMMAND ${CMAKE_COMMAND} -E env
        DTERM_SERVER=$<TARGET_FILE:dterm>
        DTERM_CLIENT=$<TARGET_FILE:dterm-client>
        python3 ${CMAKE_SOURCE_DIR}/tests/integration/run.py
)
set_tests_properties(integration PROPERTIES TIMEOUT 600)
