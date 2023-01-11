#pragma once

#include <string>
#include <functional>
#include <span>
#include <cstdint>

#include "lwip/err.h"
#include "lwip/pbuf.h"

#define BUF_SIZE 2048
#define POLL_TIME_S 2

class tcp_base {
public:
    virtual bool init() = 0;
    virtual int available() const = 0;
    virtual size_t read(std::span<uint8_t> out) = 0;
    virtual bool write(std::span<const uint8_t> data) = 0;
    virtual bool connect(std::string host, uint16_t port) = 0;
    virtual err_t close() = 0;

    virtual bool ready() const = 0;
    virtual bool initialized() const = 0;

    virtual void on_receive(std::function<void()> callback) = 0;
    virtual void on_connected(std::function<void()> callback) = 0;
    virtual void on_closed(std::function<void()> callback) = 0;
};