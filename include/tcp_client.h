#pragma once

#include <string>
#include <functional>
#include <span>

#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/pbuf.h"

#include "circular_buffer.h"
#include "logger.h"

#define BUF_SIZE 2048
#define POLL_TIME_S 2

class tcp_client {
public:
    tcp_client();
    bool init();
    int available() const;
    size_t read(std::span<uint8_t> &out);
    bool write(std::span<const uint8_t> data);
    bool connect(ip_addr_t addr, uint16_t port);
    bool connect(std::string addr, uint16_t port);
    err_t close();

    bool ready() const;
    bool initialized() const;

    void on_receive(std::function<void()> callback) {
        user_receive_callback = callback;
    }

    void on_connected(std::function<void()> callback) {
        user_connected_callback = callback;
    }

    void on_closed(std::function<void()> callback) {
        user_closed_callback = callback;
    }

protected:
    struct tcp_pcb *tcp_controlblock;
    ip_addr_t remote_addr;
    circular_buffer<uint8_t> buffer{BUF_SIZE};
    int buffer_len;
    int sent_len;
    bool connected, initialized_;
    uint16_t port_;
    std::function<void()> user_receive_callback, user_connected_callback, user_closed_callback;

    bool connect();

    static void dns_callback(const char* name, const ip_addr_t *addr, void* arg);
    static err_t poll_callback(void* arg, tcp_pcb* pcb);
    static err_t sent_callback(void* arg, tcp_pcb* pcb, u16_t len);
    static err_t recv_callback(void* arg, tcp_pcb* pcb, pbuf* p, err_t err);
    static void tcp_perror(err_t err);
    static void err_callback(void* arg, err_t err);
    static err_t connected_callback(void* arg, tcp_pcb* pcb, err_t err);
};