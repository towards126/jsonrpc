cmake_minimum_required(VERSION 3.29)
project(jsonrpc)

set(CMAKE_CXX_STANDARD 20)
find_package(jsoncpp CONFIG REQUIRED)
add_executable(jsonrpc main.cpp
        JsonRpcProtocol.h
        JsonRpcServer.h
        JsonRpcClient.h)
target_link_libraries(jsonrpc PRIVATE jsoncpp_lib)