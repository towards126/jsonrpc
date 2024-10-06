//
// Created by ljf on 24-10-5.
//
#ifndef JSON_RPC_CLIENT_H
#define JSON_RPC_CLIENT_H

#include <iostream>
#include <jsoncpp/json/json.h>
#include "JsonRpcProtocol.h"

class JsonRpcClient {
public:
    std::string sendRequest(const std::string& method, const Json::Value& params) {
        static int id=1;
        Json::Value request = JsonRpcProtocol::createRequest(method, params, id);
        id++;
        // 在实际项目中，你可能需要通过网络发送请求，这里简化为本地输出
        return request.toStyledString();
    }

    Json::Value parseResponse(const std::string& responseStr) {
        Json::CharReaderBuilder readerBuilder;
        Json::Value response;
        std::string errs;

        std::istringstream ss(responseStr);
        if (!Json::parseFromStream(readerBuilder, ss, &response, &errs)) {
            throw std::runtime_error("Failed to parse response");
        }

        if (response.isMember("error")) {
            throw std::runtime_error(response["error"]["message"].asString());
        }

        return response["result"];
    }
};

#endif // JSON_RPC_CLIENT_H
