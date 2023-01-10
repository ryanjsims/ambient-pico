#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <cstring>

#include "circular_buffer.h"
#include "tcp_tls_client.h"
#include "logger.h"
class eio_client;

extern "C" {
    int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen);
}

namespace ws {
    constexpr uint8_t final_fragment = 0x80;
    constexpr uint8_t additional_fragment = 0x00;
    constexpr uint8_t masked = 0x80;
    constexpr uint8_t has_length_16 = 0x7E;
    constexpr uint8_t has_length_64 = 0x7F;
    enum class opcodes : uint8_t {
        continuation,
        text,
        binary,
        connection = 8u,
        ping,
        pong
    };

    class websocket {
    public:
        friend class ::eio_client;
        websocket(tcp_tls_client *socket);

        void write_text(std::span<uint8_t> data);
        void write_binary(std::span<uint8_t> data);

        size_t read(std::span<uint8_t> data);
        uint32_t received_packet_size();

        void on_receive(std::function<void()> callback);
        void on_closed(std::function<void()> callback);

    private:
        tcp_tls_client *tcp;
        std::function<void()> user_receive_callback, user_close_callback;
        uint32_t packet_size;

        void mask(std::span<uint8_t> data, uint32_t masking_key);
        void tcp_recv_callback();
        void tcp_close_callback();
        void write_frame(std::span<uint8_t> data, opcodes opcode);
    };
}