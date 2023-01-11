#pragma once

#include "tcp_base.h"
#include "circular_buffer.h"
#include "logger.h"

#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"

static struct altcp_tls_config *tls_config = nullptr;

class tcp_tls_client : public tcp_base {
public:
    tcp_tls_client();
    bool init() override;
    int available() const override;
    size_t read(std::span<uint8_t> out) override;
    bool write(std::span<const uint8_t> data) override;
    bool connect(std::string host, uint16_t port) override;
    err_t close() override;

    bool ready() const override;
    bool initialized() const override;

    void on_receive(std::function<void()> callback) override {
        user_receive_callback = callback;
    }

    void on_connected(std::function<void()> callback) override {
        user_connected_callback = callback;
    }

    void on_closed(std::function<void()> callback) override {
        user_closed_callback = callback;
    }

private:
    altcp_pcb *tcp_controlblock;
    ip_addr_t remote_addr;
    circular_buffer<uint8_t> buffer{BUF_SIZE};
    int buffer_len;
    int sent_len;
    bool connected, initialized_;
    uint16_t port_;
    std::function<void()> user_receive_callback, user_connected_callback, user_closed_callback;

    bool connect();
    static void dns_callback(const char* name, const ip_addr_t *addr, void* arg);
    static err_t connected_callback(void* arg, altcp_pcb* pcb, err_t err);
    static err_t recv_callback(void* arg, altcp_pcb* pcb, pbuf* p, err_t err);
    static err_t poll_callback(void* arg, altcp_pcb* pcb);
    static err_t sent_callback(void* arg, altcp_pcb* pcb, uint16_t len);
    static void tcp_perror(err_t err);
    static void err_callback(void* arg, err_t err);
};