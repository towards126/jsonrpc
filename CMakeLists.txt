cmake_minimum_required(VERSION 3.28.1)
project(jsonrpc)

set(CMAKE_CXX_STANDARD 20)
find_package(jsoncpp CONFIG REQUIRED)
find_package(cppzmq REQUIRED)
add_executable(jsonrpc main.cpp
        JsonRpcProtocol.h
        JsonRpcServer.h
        JsonRpcClient.h
        log/blockqueue.h
        log/buffer.h
        log/log.h
        log/log.cpp
        log/buffer.cpp

)
target_link_libraries(jsonrpc PRIVATE jsoncpp_lib libzmq)