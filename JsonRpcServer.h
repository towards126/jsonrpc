#ifndef JSON_RPC_SERVER_H
#define JSON_RPC_SERVER_H

#include <json/json.h>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <future>
#include <zmq.hpp>
#include <thread>

#include "log/log.h"
#include "JsonRpcProtocol.h"

template<typename T>
T fromJson(const Json::Value &value);

template<>
int fromJson<int>(const Json::Value &value) {
    return value.asInt();
}

template<>
double fromJson<double>(const Json::Value &value) {
    return value.asDouble();
}

template<>
std::string fromJson<std::string>(const Json::Value &value) {
    return value.asString();
}


template<typename T>
struct function_traits;

// 对于普通函数
template<typename R, typename... Args>
struct function_traits<R(Args...)> {
    using return_type = R;

    template<std::size_t N>
    using arg = typename std::tuple_element_t<N, std::tuple<Args...>>;

    static constexpr std::size_t arity = sizeof...(Args);
};

// 对于 lambda 或 std::function 对象
template<typename R, typename... Args>
struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)> {
};

// 支持 std::function 类型
template<typename R, typename... Args>
struct function_traits<std::function<R(Args...)>> : public function_traits<R(Args...)> {
};
// 对于成员函数
template<typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...)> {
    using return_type = R;

    template<std::size_t N>
    using arg = typename std::tuple_element_t<N, std::tuple<Args...>>;

    static constexpr std::size_t arity = sizeof...(Args);

    using class_type = C;
};

// 用于从 Json::Value 中提取参数，并将其传递给成员函数
template<typename Instance, typename Func, typename... Args, std::size_t... Is>
auto invokeWithJsonImpl(Instance *instance, Func func, const Json::Value &params, std::index_sequence<Is...>) {
    // 使用索引展开，将 JSON 中的参数逐个传递给 fromJson
    return (instance->*func)(fromJson<Args>(params[(int) Is])...);
}

// 普通函数的实现
template<typename Func, typename... Args, std::size_t... Is>
auto invokeWithJsonImpl(Func func, const Json::Value &params, std::index_sequence<Is...>) {
    return func(fromJson<Args>(params[(int) Is])...);
}

// 生成索引序列，并调用 invokeWithJsonImpl
template<typename Instance, typename Func, typename... Args>
auto invokeWithJson(Instance *instance, Func func, const Json::Value &params) {
    return invokeWithJsonImpl<Instance, Func, Args...>(instance, func, params, std::index_sequence_for<Args...>{});
}

// 普通函数的实现
template<typename Func, typename... Args>
auto invokeWithJson(Func func, const Json::Value &params) {
    return invokeWithJsonImpl<Func, Args...>(func, params, std::index_sequence_for<Args...>{});
}

// 为成员函数指针添加支持
template<typename Instance, typename Func, std::size_t... Is>
auto invoke(Instance *instance, Func func, const Json::Value &params, std::index_sequence<Is...>) {
    using traits = function_traits<Func>;
    return invokeWithJson<Instance, Func, typename traits::template arg<Is>...>(instance, func, params);
}

// 普通函数的实现
template<typename Func, std::size_t... Is>
auto invoke(Func func, const Json::Value &params, std::index_sequence<Is...>) {
    using traits = function_traits<Func>;
    return invokeWithJson<Func, typename traits::template arg<Is>...>(func, params);
}


class JsonRpcServer {
public:

    using RpcMethod = std::function<Json::Value(const Json::Value &)>;

    explicit JsonRpcServer() {
        Log::Instance()->init(0);
        LOG_INFO("server start")
    }

    void run();

    // 注册任意函数
    template<typename F>
    void registerMethod(const std::string &method, F func, bool overwrite = false,
                        const std::string &requiredPermission = "");

    template<typename F, typename S>
    void registerMethod(const std::string &method, F func, S *s, bool overwrite = false,
                        const std::string &requiredPermission = "");

    bool send(zmq::message_t &data);

    bool recv(zmq::message_t &data);

    void as_server(int port);

private:
    struct RpcMethodInfo {
        RpcMethod method;
        std::string requiredPermission;
    };

    // 异步处理请求
    std::string handleRequestAsync(const Json::Value &request);

    std::string getAsyncResult(int requestId);

    bool checkPermission(const std::string &method, const std::string &userPermission) {
        auto methodInfo = methods[method];
        if (methodInfo.requiredPermission.empty()) return true;
        return userPermission == methodInfo.requiredPermission;
    }

    // 处理请求
    std::string handleRequest(const Json::Value &request);

    std::string handleBatchRequest(const Json::Value &batchRequest);

    std::string process(const std::string &requestStr);

private:
    std::unordered_map<std::string, RpcMethodInfo> methods;
    std::unordered_map<int, std::future<Json::Value>> async_result;
    std::mutex async_mutex;
    zmq::context_t m_context;
    std::unique_ptr<zmq::socket_t> m_socket;
    BlockQueue<std::string> requests;
    BlockQueue<std::string> responses;

};


std::string JsonRpcServer::handleRequestAsync(const Json::Value &request) {
    std::string method = request["method"].asString();
    std::string userPermission= request["userPermission"].asString();
    if (methods.find(method) == methods.end()) {
        return JsonRpcProtocol::createErrorResponse(-32601, "Method not found",
                                                    request["id"].asInt()).toStyledString();
    }
    if (!checkPermission(method, userPermission)) {
        return JsonRpcProtocol::createErrorResponse(-32001, "Permission denied", request["id"].asInt()).toStyledString();
    }
    try {
        // 异步调用方法并返回 future
        auto futureResult = std::async(std::launch::async, methods[method].method, request["params"]);
        {
            std::lock_guard<std::mutex> lock(async_mutex);
            async_result[request["id"].asInt()] = std::move(futureResult);
        }

        return JsonRpcProtocol::createResponse("Task accepted", request["id"].asInt()).toStyledString();
    } catch (const std::exception &e) {
        return JsonRpcProtocol::createErrorResponse(-32603, "Async internal error: " + std::string(e.what()),
                                                    request["id"].asInt()).toStyledString();
    }
}

std::string JsonRpcServer::handleRequest(const Json::Value &request) {


    std::string method = request["method"].asString();
    std::string userPermission= request["userPermission"].asString();
    if (method == "getAsyncResult") {
        return getAsyncResult(request["params"].asInt());
    }
    if (methods.find(method) == methods.end()) {
        return JsonRpcProtocol::createErrorResponse(-32601, "Method not found",
                                                    request["id"].asInt()).toStyledString();
    }

    if (!checkPermission(method, userPermission)) {
        return JsonRpcProtocol::createErrorResponse(-32001, "Permission denied", request["id"].asInt()).toStyledString();
    }
    try {
        Json::Value result = methods[method].method(request["params"]);

        auto response = JsonRpcProtocol::createResponse(result, request["id"].asInt()).toStyledString();
        return response;
    } catch (const std::invalid_argument &e) {
        return JsonRpcProtocol::createErrorResponse(-32602, "Invalid parameters: " + std::string(e.what()),
                                                    request["id"].asInt()).toStyledString();
    } catch (const zmq::error_t &e) {
        return JsonRpcProtocol::createErrorResponse(-32000, "ZeroMQ error: " + std::string(e.what()),
                                                    request["id"].asInt()).toStyledString();
    } catch (const std::exception &e) {
        return JsonRpcProtocol::createErrorResponse(-32603, "Internal error: " + std::string(e.what()),
                                                    request["id"].asInt()).toStyledString();
    }
}

std::string JsonRpcServer::getAsyncResult(int requestId) {
    std::lock_guard<std::mutex> lock(async_mutex);
    if (async_result.find(requestId) == async_result.end()) {
        return JsonRpcProtocol::createErrorResponse(-32602, "Task not found", requestId).toStyledString();
    }

    auto &future = async_result[requestId];
    if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        Json::Value result = future.get();
        async_result.erase(requestId);
        return JsonRpcProtocol::createResponse(result, requestId).toStyledString();
    } else {
        return JsonRpcProtocol::createResponse("Task still processing", requestId).toStyledString();
    }
}

bool JsonRpcServer::send(zmq::message_t &data) {
    try {
        m_socket->send(data, zmq::send_flags::none);
        return true;
    } catch (const zmq::error_t &e) {
        LOG_ERROR(std::format("ZMQ send error: {}", e.what()).c_str());
        return false;
    }
}

bool JsonRpcServer::recv(zmq::message_t &data) {
    try {
        m_socket->recv(data);
        return true;
    } catch (const zmq::error_t &e) {
        LOG_ERROR(std::format("ZMQ recv error: {}", e.what()).c_str());
        return false;
    }
}

void JsonRpcServer::as_server(int port) {
    m_socket = std::make_unique<zmq::socket_t>(m_context, ZMQ_REP);
    std::ostringstream os;
    os << "tcp://*:" << port;
    m_socket->bind(os.str());
}

void JsonRpcServer::run() {
    while (true) {
        zmq::message_t data;
        recv(data);
        std::string request((char *) data.data(), data.size());
        std::string response = process(request);
        zmq::message_t retmsg(response.length());
        memcpy(retmsg.data(), response.data(), response.size());
        send(retmsg);
    }
}

std::string JsonRpcServer::process(const std::string &requestStr) {
    LOG_DEBUG(requestStr.c_str())
    Json::CharReaderBuilder readerBuilder;
    Json::Value request;
    std::string errs;
    std::string result;
    std::istringstream ss(requestStr);
    if (!Json::parseFromStream(readerBuilder, ss, &request, &errs)) {
        result = JsonRpcProtocol::createErrorResponse(-32700, "Parse error", 0).toStyledString();
        return result;
    }
    if (request.isArray()) {
        return handleBatchRequest(request);
    }
    if (!request.isMember("method") || !request["method"].isString()) {
        result = JsonRpcProtocol::createErrorResponse(-32600, "Invalid Request",
                                                      request["id"].asInt()).toStyledString();
        return result;
    }

    if (!request["async"].asBool()) {
        result = handleRequest(request);
    } else {
        result = handleRequestAsync(request);
    }
    LOG_DEBUG(result.c_str());
    return result;
}

std::string JsonRpcServer::handleBatchRequest(const Json::Value &batchRequest) {


    Json::Value batchResponse(Json::arrayValue);
    for (const auto &request: batchRequest) {
        std::string result;
        if (!request["async"].asBool()) {
            result = handleRequest(request);
        } else {
            result = handleRequestAsync(request);
        }
        Json::Value response;
        std::istringstream(result) >> response;
        batchResponse.append(response);
    }

    return batchResponse.toStyledString();
}

// 成员函数版本
template<typename Func, typename C>
void JsonRpcServer::registerMethod(const std::string &method, Func func, C *instance, bool overwrite,
                                   const std::string &requiredPermission) {
    if (method == "getAsyncResult") {
        std::cerr << "getAsyncResult is used" << std::endl;
        return;
    }

    using traits = function_traits<Func>;

    LOG_DEBUG(std::format("register method :{}", method).c_str())
    if (methods.find(method) != methods.end() && !overwrite) {
        throw std::runtime_error("Method already registered");
    }

    // 创建包装器，将 JSON 参数转换为函数所需的参数
    auto wrapper = [instance, func](const Json::Value &params) -> Json::Value {
        if (params.size() != traits::arity) {
            throw std::runtime_error("Incorrect number of arguments");
        }

        // 调用成员函数并将 instance 和 func 传递给 invoke
        return invoke<C, Func>(instance, func, params, std::make_index_sequence<traits::arity>{});
    };

    methods[method] = {wrapper, requiredPermission};
}


template<typename Func>
void JsonRpcServer::registerMethod(const std::string &method, Func func, bool overwrite,
                                   const std::string &requiredPermission) {
    if (method == "getAsyncResult") {
        std::cerr << "getAsyncResult is used" << std::endl;
        return;
    }
    using traits = function_traits<Func>;
    LOG_DEBUG(std::format("register method :{}", method).c_str())
    if (methods.find(method) != methods.end() && !overwrite) {
        throw std::runtime_error("Method already registered");
    }
    // 创建包装器，将 JSON 参数转换为函数所需的参数
    auto wrapper = [func](const Json::Value &params) -> Json::Value {
        if (params.size() != traits::arity) {
            throw std::runtime_error("Incorrect number of arguments");
        }

        //return invokeWithJson<Func, typename traits::template arg<0>, typename traits::template arg<1> /*...更多参数*/>(func, params);
        return invoke<Func>(func, params, std::make_index_sequence<traits::arity>{});

    };

    methods[method] = {wrapper, requiredPermission};

}

#endif // JSON_RPC_SERVER_H
