#ifndef JSON_RPC_SERVER_H
#define JSON_RPC_SERVER_H

#include <json/json.h>
#include <unordered_map>
#include <functional>
#include <iostream>
#include "JsonRpcProtocol.h"
#include <tuple>
#include <type_traits>

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
    using arg = typename std::tuple_element<N, std::tuple<Args...>>::type;

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

template<typename Func, typename Tuple, std::size_t... Is>
auto invokeWithTuple(Func func, const Tuple& params, std::index_sequence<Is...>) {
    // 使用 std::get 获取 tuple 的每个参数，并传递给函数
    return func(std::get<Is>(params)...);
}

#include <tuple>
#include <utility> // for std::index_sequence

// 用于从 Json::Value 中提取参数，并将其传递给函数
template<typename Func, typename... Args, std::size_t... Is>
auto invokeWithJsonImpl(Func func, const Json::Value& params, std::index_sequence<Is...>) {
    // 使用索引展开，将 JSON 中的参数逐个传递给 fromJson
    return func(fromJson<Args>(params[(int)Is])...);
}

// 生成索引序列，并调用 invokeWithJsonImpl
template<typename Func, typename... Args>
auto invokeWithJson(Func func, const Json::Value& params) {
    // std::index_sequence_for 自动生成索引序列
    return invokeWithJsonImpl<Func, Args...>(func, params, std::index_sequence_for<Args...>{});
}



class JsonRpcServer {
public:
    using RpcMethod = std::function<Json::Value(const Json::Value &)>;

    // 注册任意函数
    template<typename Func>
    void registerMethod(const std::string& method, Func func) {
        using traits = function_traits<Func>;

        // 创建包装器，将 JSON 参数转换为函数所需的参数
        auto wrapper = [func](const Json::Value& params) -> Json::Value {
            if (params.size() != traits::arity) {
                throw std::runtime_error("Incorrect number of arguments");
            }

            return invokeWithJson<Func, typename traits::template arg<0>, typename traits::template arg<1> /*...更多参数*/>(func, params);
        };

        methods[method] = wrapper;
    }


    // 处理请求
    std::string handleRequest(const std::string &requestStr) {
        Json::CharReaderBuilder readerBuilder;
        Json::Value request;
        std::string errs;

        std::istringstream ss(requestStr);
        if (!Json::parseFromStream(readerBuilder, ss, &request, &errs)) {
            return JsonRpcProtocol::createErrorResponse(-32700, "Parse error", 0).toStyledString();
        }

        if (!request.isMember("method") || !request["method"].isString()) {
            return JsonRpcProtocol::createErrorResponse(-32600, "Invalid Request",
                                                        request["id"].asInt()).toStyledString();
        }

        std::string method = request["method"].asString();
        if (methods.find(method) == methods.end()) {
            return JsonRpcProtocol::createErrorResponse(-32601, "Method not found",
                                                        request["id"].asInt()).toStyledString();
        }

        try {
            Json::Value result = methods[method](request["params"]);
            return JsonRpcProtocol::createResponse(result, request["id"].asInt()).toStyledString();
        } catch (const std::exception &e) {
            return JsonRpcProtocol::createErrorResponse(-32603, "Internal error",
                                                        request["id"].asInt()).toStyledString();
        }
    }

private:
    std::unordered_map<std::string, RpcMethod> methods;
};

#endif // JSON_RPC_SERVER_H
