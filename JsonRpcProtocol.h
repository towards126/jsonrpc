//
// Created by ljf on 24-10-5.
//

#ifndef JSON_RPC_PROTOCOL_H
#define JSON_RPC_PROTOCOL_H

#include <jsoncpp/json/json.h>
#include <string>
template<typename T>
T fromJson(const Json::Value& value) {
    // 这里可以根据 T 的类型决定如何从 Json::Value 中提取
    if constexpr (std::is_same_v<T, int>) {
        return value.asInt();
    } else if constexpr (std::is_same_v<T, std::string>) {
        return value.asString();
    }
    // 可以添加更多类型支持
}


class JsonRpcProtocol {
public:
    static Json::Value createRequest(const std::string& method, const Json::Value& params, int id) {
        Json::Value request;
        request["jsonrpc"] = "2.0";
        request["method"] = method;
        request["params"] = params;
        request["id"] = id;
        return request;
    }

    static Json::Value createResponse(const Json::Value& result, int id) {
        Json::Value response;
        response["jsonrpc"] = "2.0";
        response["result"] = result;
        response["id"] = id;
        return response;
    }

    static Json::Value createErrorResponse(int code, const std::string& message, int id) {
        Json::Value response;
        response["jsonrpc"] = "2.0";
        response["error"]["code"] = code;
        response["error"]["message"] = message;
        response["id"] = id;
        return response;
    }
};

#endif // JSON_RPC_PROTOCOL_H

