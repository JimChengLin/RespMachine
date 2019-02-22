cmake_minimum_required(VERSION 3.8)
project(RespMachine)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

add_executable(RespMachine main.cpp
        src/resp_machine.cpp
        src/resp_machine.h)