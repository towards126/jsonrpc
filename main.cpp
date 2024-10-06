#include <iostream>
#include "JsonRpcServer.h"
#include "JsonRpcClient.h"

using namespace std;

int add(int a,int b,int c){
    return a+b;
}

int main() {
    JsonRpcServer server;

// 注册具有不同参数的函数
    server.registerMethod("add", add);

    std::string requestAdd = R"({"jsonrpc": "2.0", "method": "add", "params": [3, 4], "id": 1})";
    std::string responseAdd = server.handleRequest(requestAdd);
    std::cout << "Response: " << responseAdd << std::endl;

    std::string requestConcat = R"({"jsonrpc": "2.0", "method": "concat", "params": ["Hello, ", "world!"], "id": 2})";
    std::string responseConcat = server.handleRequest(requestConcat);
    std::cout << "Response: " << responseConcat << std::endl;

    return 0;
}
