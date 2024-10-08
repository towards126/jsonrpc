//
// Created by ljf on 24-9-9.
//

#include "buffer.h"

Buffer::Buffer(int initBuffSize) : m_buffer(initBuffSize), m_readPos(0), m_writePos(0) {

}

size_t Buffer::WritableBytes() const {
    return m_buffer.size() - m_writePos;
}

size_t Buffer::ReadableBytes() const {
    return m_writePos - m_readPos;
}

size_t Buffer::PrependableBytes() const {
    return m_readPos;
}

const char *Buffer::Peek() const {
    return BeginPtr_() + m_readPos;
}

void Buffer::EnsureWriteable(size_t len) {
    if (WritableBytes() < len) {
        MakeSpace_(len);
    }
}

void Buffer::HasWritten(size_t len) {
    m_writePos += len;
}

void Buffer::Retrieve(size_t len) {
    m_readPos += len;
}

void Buffer::RetrieveUntil(const char *end) {
    Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {
    bzero(&*m_buffer.begin(), m_buffer.size());
    m_readPos = m_writePos = 0;
}

std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

const char *Buffer::BeginWriteConst() const {
    return BeginPtr_() + m_writePos;
}

char *Buffer::BeginWrite() {
    return BeginPtr_() + m_writePos;
}

void Buffer::Append(const std::string &str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const char *str, size_t len) {
    EnsureWriteable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const void *data, size_t len) {
    Append(static_cast<const char *>(data), len);
}

void Buffer::Append(const Buffer &buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

ssize_t Buffer::ReadFd(int fd, int *Errno) {
    char buff[65535];
    struct iovec iovec[2];
    const size_t writable = WritableBytes();
    iovec[0].iov_base = BeginPtr_() + m_writePos;
    iovec[0].iov_len = writable;
    iovec[1].iov_base = buff;
    iovec[1].iov_len = sizeof buff;
    auto len = readv(fd, iovec, 2);
    if (len < 0) {
        *Errno = errno;
    } else if (static_cast<size_t>(len) <= writable) {
        m_writePos += len;
    } else {
        m_writePos = m_buffer.size();
        Append(buff, len - writable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int *Errno) {
    auto readSize = ReadableBytes();
    auto len = write(fd, Peek(), readSize);
    if (len < 0) {
        *Errno = errno;
        return len;
    }
    m_readPos += len;
    return len;
}

char *Buffer::BeginPtr_() {
    return &*m_buffer.begin();
}

const char *Buffer::BeginPtr_() const {
    return &*m_buffer.begin();
}

void Buffer::MakeSpace_(size_t len) {
    if (WritableBytes() + PrependableBytes() < len) {
        m_buffer.resize(m_writePos + len * 2 + 1);
    } else {
        auto readable = ReadableBytes();
        std::copy(BeginPtr_() + m_readPos, BeginPtr_() + m_writePos, BeginPtr_());
        m_readPos = 0;
        m_writePos = m_readPos + readable;

    }
}
