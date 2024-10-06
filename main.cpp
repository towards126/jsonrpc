#include <iostream>
#include "JsonRpcServer.h"
#include "JsonRpcClient.h"

using namespace std;

// 定义一些 RPC 方法
Json::Value addMethod(const Json::Value &params) {
    return params[0].asInt() + params[1].asInt();
}

Json::Value subtractMethod(const Json::Value &params) {
    return params[0].asInt() - params[1].asInt();
}

Json::Value foo_3(const Json::Value &params) {
    return Json::Value{};
}

int main() {
    // 创建服务端并注册方法
    JsonRpcServer server;
    server.registerMethod("add", addMethod);
    server.registerMethod("subtract", subtractMethod);
    server.registerMethod("mul", function<Json::Value(Json::Value)>(foo_3));

    // 模拟客户端发送请求
    JsonRpcClient client;
    Json::Value iNum;
    iNum[0] = 10;
    iNum[1] = 5;
    iNum[2]="t";
    std::string request = client.sendRequest("mul", iNum);

    cout<<request<<endl;
    // 服务端处理请求
    std::string response = server.handleRequest(request);

    // 客户端解析响应
    try {
        Json::Value result = client.parseResponse(response);
        std::cout << "Result: " << result.asInt() << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    request = client.sendRequest("subtract", iNum);

    cout<<request<<endl;
    // 服务端处理请求
    response = server.handleRequest(request);

    // 客户端解析响应
    try {
        Json::Value result = client.parseResponse(response);
        std::cout << "Result: " << result.asInt() << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
