//
// Created by ljf on 24-9-9.
//

#ifndef UNTITLED5_BLOCKQUEUE_H
#define UNTITLED5_BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>
#include <cassert>

template<class T>
class BlockQueue {
public:
    explicit BlockQueue(size_t maxCapacity = 1000);

    ~BlockQueue();

    void clear();

    bool empty();

    bool full();

    void close();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_back(const T &item);

    void push_front(const T &item);

    bool pop(T &item);

    bool pop(T &item, int timeout);

    void flush();


private:
    std::deque<T> m_deq;
    size_t m_capacity = 1000;
    std::mutex m_mtx;
    bool m_isclose;
    std::condition_variable m_condConsumer;
    std::condition_variable m_condProducer;
};

template<class T>
void BlockQueue<T>::flush() {
    m_condConsumer.notify_one();
}

template<class T>
bool BlockQueue<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> lockGuard(m_mtx);
    while (m_deq.empty()) {
        if (m_condConsumer.wait_for
                (lockGuard, std::chrono::seconds(timeout)) == std::cv_status::timeout) {
            return false;
        }
        if (m_isclose) return false;
    }
    item = m_deq.front();
    m_deq.pop_front();
    m_condProducer.notify_one();
    return true;
}

template<class T>
bool BlockQueue<T>::pop(T &item) {
    std::unique_lock<std::mutex> lockGuard(m_mtx);
    while (m_deq.empty()) {
        m_condConsumer.wait(lockGuard);
        if (m_isclose) return false;
    }
    item = m_deq.front();
    m_deq.pop_front();
    m_condProducer.notify_one();
    return true;
}

template<class T>
void BlockQueue<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> lockGuard(m_mtx);
    while (m_deq.size() >= m_capacity) {
        m_condConsumer.wait(lockGuard);
    }
    m_deq.push_front(item);
    m_condConsumer.notify_one();
}

template<class T>
void BlockQueue<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> lockGuard(m_mtx);
    while (m_deq.size() >= m_capacity) {
        m_condConsumer.wait(lockGuard);
    }
    m_deq.push_back(item);
    m_condConsumer.notify_one();
}


template<class T>
T BlockQueue<T>::back() {
    std::lock_guard<std::mutex> lockGuard(m_mtx);
    return m_deq.back();
}

template<class T>
T BlockQueue<T>::front() {
    std::lock_guard<std::mutex> lockGuard(m_mtx);
    return m_deq.front();
}

template<class T>
size_t BlockQueue<T>::capacity() {
    std::lock_guard<std::mutex> lockGuard(m_mtx);
    return m_capacity;
}

template<class T>
size_t BlockQueue<T>::size() {
    std::lock_guard<std::mutex> lockGuard(m_mtx);
    return m_deq.size();
}

template<class T>
bool BlockQueue<T>::full() {
    std::lock_guard<std::mutex> lockGuard(m_mtx);
    return m_deq.size() >= m_capacity;
}

template<class T>
bool BlockQueue<T>::empty() {
    std::lock_guard<std::mutex> lockGuard(m_mtx);
    return m_deq.empty();
}

template<class T>
void BlockQueue<T>::clear() {
    std::lock_guard<std::mutex> lockGuard(m_mtx);
    m_deq.clear();
}

template<class T>
void BlockQueue<T>::close() {
    {
        std::lock_guard<std::mutex> lockGuard(m_mtx);
        m_deq.clear();
        m_isclose = true;
    }
    m_condConsumer.notify_all();
    m_condProducer.notify_all();
}

template<class T>
BlockQueue<T>::~BlockQueue() {
    close();
}

template<class T>
BlockQueue<T>::BlockQueue(size_t maxCapacity) {
    assert(maxCapacity > 0);
    m_isclose = false;
}


#endif //UNTITLED5_BLOCKQUEUE_H
