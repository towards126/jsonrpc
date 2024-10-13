//
// Created by ljf on 24-10-5.
//
#ifndef JSON_RPC_CLIENT_H
#define JSON_RPC_CLIENT_H

#include <iostream>
#include <jsoncpp/json/json.h>
#include <zmq.hpp>
#include "JsonRpcProtocol.h"

class JsonRpcClient {
public:
    void connect(const std::string &ip, int port);

    void send(zmq::message_t &data);

    void recv(zmq::message_t &data);

    std::string call(const std::string& call);

    std::string sendRequest(const std::string &method, const Json::Value &params, bool async = false,
                            const std::string& userPermission="") {
        static int id = 1;
        Json::Value request = JsonRpcProtocol::createRequest(method, params, id, async,userPermission);
        id++;
        return request.toStyledString();
    }

    Json::Value parseResponse(const std::string &responseStr) {
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

private:
    zmq::context_t m_context;
    std::unique_ptr<zmq::socket_t> m_socket;
};

void JsonRpcClient::connect(const std::string &ip, int port) {
    m_socket = std::make_unique<zmq::socket_t>(m_context, ZMQ_REQ);
    std::ostringstream os;
    os << "tcp://" << ip << ":" << port;
    m_socket->connect(os.str());
}

void JsonRpcClient::send(zmq::message_t &data) {
    zmq::send_flags flag = zmq::send_flags::dontwait;
    m_socket->send(data, flag);
}

void JsonRpcClient::recv(zmq::message_t &data) {
    m_socket->recv(data);
}

std::string JsonRpcClient::call(const std::string& call) {
    zmq::message_t request(call.size() + 1);
    memcpy(request.data(),call.data(),call.size());
    send(request);
    zmq::message_t reply;
    recv(reply);
    if (reply.empty()) {
        return JsonRpcProtocol::createErrorResponse(
                -32603,"empty response",-1).toStyledString();
    }
    return reply.to_string();
}

#endif // JSON_RPC_CLIENT_H
