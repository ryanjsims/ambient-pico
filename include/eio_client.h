#pragma once

#include <cstdint>
#include "websocket.h"

class eio_client {
public:
    enum class packet_type: uint8_t {
        open = '0',
        close,
        ping,
        pong,
        message,
        upgrade,
        noop
    };

    eio_client(ws::websocket *socket);
    eio_client(tcp_base *socket);
    ~eio_client() { 
        trace1("~eio_client\n");
        delete socket_;
    }

    size_t read(std::span<uint8_t> data);
    // Needs 1 + 14 add'l bytes to encode message
    bool send_message(std::span<uint8_t> data);
    uint32_t packet_size() const;

    void on_open(std::function<void()> callback);
    void on_receive(std::function<void()> callback);
    void on_closed(std::function<void(err_t)> callback);

    void read_initial_packet();

private:
    ws::websocket *socket_;
    std::function<void()> user_receive_callback, user_open_callback;
    std::function<void(err_t)> user_close_callback;
    std::string sid;
    int ping_interval, ping_timeout, ping_milliseconds;
    bool open_;

    void ws_recv_callback();
    void ws_poll_callback();
    void ws_close_callback(err_t reason);
};