//
// Created by ljf on 24-9-9.
//

#include "log.h"

void Log::init(int level, const char *path, const char *suffix, int maxQueueCapacity) {
    isOpen_ = true;
    level_ = level;
    if (maxQueueCapacity > 0) {
        isAsync_ = true;
        if (!deque_) {
            deque_ = std::make_unique<BlockQueue<std::string>>();
            writeThread_ = std::make_unique<std::thread>(FlushLogThread);
        } else { isAsync_ = false; }
    }
    lineCount_ = 0;
    auto timer = time(nullptr);
    auto *systime = localtime(&timer);
    auto t = systime;
    path_ = path;
    suffix_ = suffix;
    /*
    char filename[LOG_NAME_LEN] = {0};
    snprintf(filename, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
             path_, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, suffix_);
             */
    toDay_ = t->tm_mday;
    auto fileName = std::format("{}/{:04d}_{:02d}_{:02d}{}", path_, t->tm_year + 1900,
                                t->tm_mon + 1, t->tm_mday, suffix);
    {
        std::lock_guard<std::mutex> lockGuard(mtx_);
        buff_.RetrieveAll();
        if (fp_) {
            flush();
            fclose(fp_);
        }
        fp_ = fopen(fileName.c_str(), "a");
        if (fp_ == nullptr) {
            mkdir(path_, 0777);
            fp_ = fopen(fileName.c_str(), "a");
        }
        assert(fp_);
    }
}

Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

Log::~Log() {

    if (writeThread_ && writeThread_->joinable()) {
        while (!deque_->empty()) {
            deque_->flush();
        };
        deque_->close();
        writeThread_->join();
    }
    if (fp_) {
        std::lock_guard<std::mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

Log *Log::Instance() {
    static Log inst;
    return &inst;
}

void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}

void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    auto tSec = now.tv_sec;
    auto *systime = localtime(&tSec);
    auto t = systime;
    va_list vaList;
    if (toDay_ != t->tm_mday || (lineCount_ % MAX_LINES == 0)) {
        std::unique_lock<std::mutex> lock(mtx_);
        lock.unlock();
        std::string newfile;
        auto tail = std::format("{:04d}_{:02d}_{:02d}", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
        if (toDay_ != t->tm_mday) {
            newfile = std::format("{}/{}{}", path_, tail, suffix_);
            toDay_ = t->tm_mday;
            lineCount_ = 0;
        } else {
            newfile = std::format("{}/{}-{}{}", path_, tail, (lineCount_ / MAX_LINES), suffix_);
        }
        lock.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newfile.c_str(), "a");
        assert(fp_);
    }
    {
        std::unique_lock<std::mutex> lock(mtx_);
        lineCount_++;
        auto n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                          t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                          t->tm_hour, t->tm_min, t->tm_sec, now.tv_usec);
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);
        va_start(vaList, format);
        auto m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);
        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);
        if (isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {
            fputs(buff_.Peek(),fp_);
        }
        buff_.RetrieveAll();
    }
}
void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
        case 0:
            buff_.Append("[debug]: ", 9);
            break;
        case 1:
            buff_.Append("[info] : ", 9);
            break;
        case 2:
            buff_.Append("[warn] : ", 9);
            break;
        case 3:
            buff_.Append("[error]: ", 9);
            break;
        default:
            buff_.Append("[info] : ", 9);
            break;
    }
}

void Log::flush() {
    if (isAsync_) deque_->flush();
    fflush(fp_);
}

int Log::GetLevel() {
    std::lock_guard<std::mutex> lockGuard(mtx_);
    return level_;
}

void Log::SetLevel(int level) {
    std::lock_guard<std::mutex> lockGuard(mtx_);
    level_=level;
}

void Log::AsyncWrite_() {
    std::string str;
    while(deque_->pop(str)) {
        std::lock_guard<std::mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}
