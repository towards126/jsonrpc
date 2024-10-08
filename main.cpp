#include <iostream>
#include "JsonRpcServer.h"
#include "JsonRpcClient.h"

using namespace std;

int add(int a,int b,int c){
    return a+b+c;
}
string concat(string a,string b)
{
    return a+b;
}
int main() {
    JsonRpcServer server;

// 注册具有不同参数的函数
    server.registerMethod("add", add);
    server.registerMethod("concat", concat);
    std::string requestAdd = R"({"jsonrpc": "2.0", "method": "add", "params": [3, 4,5], "id": 1})";
    Json::CharReaderBuilder readerBuilder;
    Json::Value jsonData;   // 用于存储解析后的 JSON 数据
    std::string errs;       // 用于存储解析错误


    std::string responseAdd = server.handleRequest(requestAdd);
    // 将 JSON 字符串转换为输入流
    std::istringstream s(responseAdd);
    Json::parseFromStream(readerBuilder, s, &jsonData, &errs);
    std::cout << "Response: " << jsonData["result"] << std::endl;

    std::string requestConcat = R"({"jsonrpc": "2.0", "method": "concat", "params": ["Hello, ", "world!"], "id": 2})";
    s.clear();


    std::string responseConcat = server.handleRequest(requestConcat);
    s.str(responseConcat);
    Json::parseFromStream(readerBuilder, s, &jsonData, &errs);
    std::cout << "Response: " << responseConcat << std::endl;
    Json::Value value;
    value[0] = "hello";
    value[1] = "world";
    JsonRpcClient client;
    string request=client.sendRequest("concat", value);
    cout<<request<<endl;
    responseAdd = server.handleRequest(request);
    cout << responseAdd;
    return 0;
}
