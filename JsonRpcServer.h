#ifndef JSON_RPC_SERVER_H
#define JSON_RPC_SERVER_H

#include <json/json.h>
#include <unordered_map>
#include <functional>
#include <iostream>
#include "JsonRpcProtocol.h"

class JsonRpcServer {
public:
    using RpcMethod = std::function<Json::Value(const Json::Value&)>;

    // 注册方法
    void registerMethod(const std::string& method, RpcMethod func) {
        methods[method] = func;
    }

    // 处理请求
    std::string handleRequest(const std::string& requestStr) {
        Json::CharReaderBuilder readerBuilder;
        Json::Value request;
        std::string errs;

        std::istringstream ss(requestStr);
        if (!Json::parseFromStream(readerBuilder, ss, &request, &errs)) {
            return JsonRpcProtocol::createErrorResponse(-32700, "Parse error", 0).toStyledString();
        }

        // 检查是否包含方法
        if (!request.isMember("method") || !request["method"].isString()) {
            return JsonRpcProtocol::createErrorResponse(-32600, "Invalid Request", request["id"].asInt()).toStyledString();
        }

        std::string method = request["method"].asString();
        if (methods.find(method) == methods.end()) {
            return JsonRpcProtocol::createErrorResponse(-32601, "Method not found", request["id"].asInt()).toStyledString();
        }

        try {
            Json::Value result = methods[method](request["params"]);
            return JsonRpcProtocol::createResponse(result, request["id"].asInt()).toStyledString();
        } catch (const std::exception& e) {
            return JsonRpcProtocol::createErrorResponse(-32603, "Internal error", request["id"].asInt()).toStyledString();
        }
    }

private:
    std::unordered_map<std::string, RpcMethod> methods;
};

#endif // JSON_RPC_SERVER_H
