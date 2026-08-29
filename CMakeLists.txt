cmake_minimum_required(VERSION 3.20)
project(dterm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dterm
        server/main.cpp
        server/terminal.cpp
        server/server.cpp
)

add_executable(dterm-client
        client/main.cpp
        client/raw_mode.cpp
)
