//
// Created by ljf on 24-9-9.
//

#ifndef UNTITLED5_LOG_H
#define UNTITLED5_LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <format>
#include <sys/time.h>
#include <cstring>
#include <cstdarg>           // vastart va_end
#include <cassert>
#include <sys/stat.h>         //mkdir
#include "blockqueue.h"
#include "buffer.h"

class Log {
public:
    void init(int level, const char *path = "./log",
              const char *suffix = ".log",
              int maxQueueCapacity = 1024);

    static Log *Instance();

    static void FlushLogThread();

    void write(int level, const char *format, ...);

    void flush();

    int GetLevel();

    void SetLevel(int level);

    bool isOpen() const { return isOpen_; }

private:
    Log();

    void AppendLogLevelTitle_(int level);

    virtual ~Log();

    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    const char *path_;
    const char *suffix_;

    int MAX_LINES_;

    int lineCount_;
    int toDay_;

    bool isOpen_;

    Buffer buff_;
    int level_;
    bool isAsync_;

    FILE *fp_;
    std::unique_ptr<BlockQueue<std::string>> deque_;
    std::unique_ptr<std::thread> writeThread_;
    std::mutex mtx_;
};

#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->isOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);
#endif //UNTITLED5_LOG_H
