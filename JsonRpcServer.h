#ifndef JSON_RPC_SERVER_H
#define JSON_RPC_SERVER_H

#include <json/json.h>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <future>

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

template<typename Func, typename Tuple, std::size_t... Is>
auto invokeWithTuple(Func func, const Tuple &params, std::index_sequence<Is...>) {
    // 使用 std::get 获取 tuple 的每个参数，并传递给函数
    return func(std::get<Is>(params)...);
}


// 用于从 Json::Value 中提取参数，并将其传递给函数
template<typename Func, typename... Args, std::size_t... Is>
auto invokeWithJsonImpl(Func func, const Json::Value &params, std::index_sequence<Is...>) {
    // 使用索引展开，将 JSON 中的参数逐个传递给 fromJson
    return func(fromJson<Args>(params[(int) Is])...);
}

// 生成索引序列，并调用 invokeWithJsonImpl
template<typename Func, typename... Args>
auto invokeWithJson(Func func, const Json::Value &params) {
    // std::index_sequence_for 自动生成索引序列
    return invokeWithJsonImpl<Func, Args...>(func, params, std::index_sequence_for<Args...>{});
}

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

    void run() {

    }

// 注册任意函数
    template<typename Func>
    void registerMethod(const std::string &method, Func func) {
        if (method=="getAsyncResult") {
            std::cerr<<"getAsyncResult is used"<<std::endl;
            return;
        }
        using traits = function_traits<Func>;
        LOG_DEBUG(std::format("register method :{}", method).c_str())

        // 创建包装器，将 JSON 参数转换为函数所需的参数
        auto wrapper = [func](const Json::Value &params) -> Json::Value {
            if (params.size() != traits::arity) {
                throw std::runtime_error("Incorrect number of arguments");
            }

            //return invokeWithJson<Func, typename traits::template arg<0>, typename traits::template arg<1> /*...更多参数*/>(func, params);
            return invoke<Func>(func, params, std::make_index_sequence<traits::arity>{});

        };

        methods[method] = wrapper;
    }
    std::string handleRequestAsync(const std::string &requestStr) {
        Json::CharReaderBuilder readerBuilder;
        Json::Value request;
        std::string errs;

        // 解析 JSON 请求
        std::istringstream ss(requestStr);
        if (!Json::parseFromStream(readerBuilder, ss, &request, &errs)) {
            return JsonRpcProtocol::createErrorResponse(-32700, "Parse error", 0).toStyledString();
        }

        // 检查方法是否存在
        std::string method = request["method"].asString();
        if (methods.find(method) == methods.end()) {
            return JsonRpcProtocol::createErrorResponse(-32601, "Method not found", request["id"].asInt()).toStyledString();
        }

        // 异步调用方法并返回 future
        auto futureResult = std::async(methods[method],request["params"]);
        {
            std::lock_guard<std::mutex> lock(async_mutex);
            async_result[request["id"].asInt()] = std::move(futureResult);
        }

        /*
        // 立即返回任务已被接受，可以通过别的机制获取完成结果
        Json::Value response;
        response["jsonrpc"] = "2.0";
        response["result"] = "Task accepted";
        response["id"] = request["id"];
         */

        return JsonRpcProtocol::createResponse("Task accepted",request["id"].asInt()).toStyledString();
    }
    std::string getAsyncResult(int requestId) {
        // 检查任务是否存在
        if (async_result.find(requestId) == async_result.end()) {
            return JsonRpcProtocol::createErrorResponse(-32602, "Task not found", requestId).toStyledString();
        }

        // 检查任务是否完成
        async_mutex.lock();
        auto &future = async_result[requestId];
        async_mutex.unlock();
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            // 获取结果
            Json::Value result = future.get();
            // 移除任务（任务已完成）
            async_result.erase(requestId);
            return JsonRpcProtocol::createResponse(result, requestId).toStyledString();
        } else {
            // 如果任务尚未完成，返回任务仍在处理中
            return JsonRpcProtocol::createResponse("Task still processing", requestId).toStyledString();
        }
    }
    // 处理请求
    std::string handleRequest(const std::string &requestStr) {
        LOG_DEBUG(requestStr.c_str())
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
        if (method=="getAsyncResult") {
            return getAsyncResult(request["params"].asInt());
        }
        if (methods.find(method) == methods.end()) {
            return JsonRpcProtocol::createErrorResponse(-32601, "Method not found",
                                                        request["id"].asInt()).toStyledString();
        }

        try {
            Json::Value result = methods[method](request["params"]);

            auto response = JsonRpcProtocol::createResponse(result, request["id"].asInt()).toStyledString();
            LOG_DEBUG(response.c_str())
            return response;
        } catch (const std::exception &e) {
            return JsonRpcProtocol::createErrorResponse(-32603, "Internal error",
                                                        request["id"].asInt()).toStyledString();
        }
    }

private:
    std::unordered_map<std::string, RpcMethod> methods;
    std::unordered_map<int,std::future<Json::Value>> async_result;
    std::mutex async_mutex;
};

#endif // JSON_RPC_SERVER_H
