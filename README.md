# JSON-RPC 服务器

## 概述
这是一个基于 C++ 的简单 **JSON-RPC 服务器**，提供了注册和通过 JSON-RPC 请求调用方法的框架。它使用 **ZeroMQ** 进行网络通信，并支持同步和异步的 RPC 方法执行。服务器还支持权限检查，并能够灵活地处理成员函数注册。

## 功能
- **动态注册和调用函数**：可以注册普通函数或成员函数，并通过 JSON-RPC 进行调用。
- **支持异步调用**：支持异步处理方法，并可以通过 `getAsyncResult` 查询结果。
- **使用 ZeroMQ 进行通信**：ZeroMQ 用作底层消息传输系统。
- **权限检查**：每个方法都可以指定所需权限，并在执行前进行验证。


使用说明
注册方法
服务器允许你注册可以通过 JSON-RPC 调用的函数。以下是如何注册方法的示例：

    

* 普通函数：
```C++
JsonRpcServer server;
server.registerMethod("add", [](int a, int b) {
    return a + b;
});
```
* 成员函数：
```C++
struct Calculator {
    int add(int a, int b) {
        return a + b;
    }
};

Calculator calc;
server.registerMethod("add", &Calculator::add, &calc);
```
* 权限控制：
你可以通过指定权限来限制方法的访问：
```C++
server.registerMethod("secureAdd", [](int a, int b) {
    return a + b;
}, false, "admin");
```
* 运行服务器
设置好方法和 ZeroMQ 套接字后，可以通过调用 run() 方法来运行服务器：
```C++
int main() {
    JsonRpcServer server;
    server.as_server(5555);  // 在端口 5555 上启动服务器
    server.run();
}
```
* 发送请求
服务器运行后，可以发送 JSON-RPC 请求。以下是一个同步请求的示例：
```
{
  "jsonrpc": "2.0",
  "method": "add",
  "params": [5, 7],
  "id": 1
}
```

*异步请求示例：
```
{
  "jsonrpc": "2.0",
  "method": "secureAdd",
  "params": [5, 7],
  "id": 2,
  "async": true,
  "userPermission": "admin"
}
```
*获取异步结果
对于异步方法，可以稍后通过以下请求获取结果：
```
{
  "jsonrpc": "2.0",
  "method": "getAsyncResult",
  "params": [2],
  "id": 3
}
```
