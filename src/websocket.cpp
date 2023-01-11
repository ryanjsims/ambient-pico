#include "websocket.h"

#include "lwip/ip_addr.h"

ws::websocket::websocket(tcp_base *socket): tcp(socket), user_receive_callback([](){}), user_close_callback([](){}) {
    tcp->on_receive(std::bind(&websocket::tcp_recv_callback, this));
    tcp->on_closed(std::bind(&websocket::tcp_close_callback, this));
}

void ws::websocket::write_text(std::span<uint8_t> data) {
    write_frame(data, opcodes::text);
}

void ws::websocket::write_binary(std::span<uint8_t> data) {
    write_frame(data, opcodes::binary);
}

size_t ws::websocket::read(std::span<uint8_t> data) {
    return tcp->read(data);
}

uint32_t ws::websocket::received_packet_size() {
    return packet_size;
}

void ws::websocket::on_receive(std::function<void()> callback) {
    user_receive_callback = callback;
}

void ws::websocket::on_closed(std::function<void()> callback) {
    user_close_callback = callback;
}

void ws::websocket::mask(std::span<uint8_t> data, uint32_t masking_key) {
    std::span<uint8_t> masking_bytes = {(uint8_t*)&masking_key, sizeof(masking_key)};
    for(uint32_t i = 0; i < data.size(); i++) {
        data[i] ^= masking_bytes[i % 4];
    }
}

void ws::websocket::tcp_recv_callback() {
    uint8_t frame_header[2];
    tcp->read({frame_header, 2});
    packet_size = frame_header[1] & 0x7F;
    debug("Header packet size: %x\n", packet_size);
    if(packet_size == 0x7E) {
        uint16_t temp;
        tcp->read({(uint8_t*)&temp, 2});
        packet_size = ntohs(temp);
    } else if(packet_size == 0x7F) {
        uint32_t temp;
        tcp->read({(uint8_t*)&temp, 4});
        tcp->read({(uint8_t*)&packet_size, 4});
        packet_size = ntohl(packet_size);
    }
    debug("ws::websocket::tcp_recv_callback: Got size %u (0x%08x)\n", packet_size, packet_size);
    switch(opcodes(frame_header[0] & 0x07)) {
    case opcodes::ping:
        // Client should never receive a ping
        break;
    case opcodes::pong:
        // Reset timeout timer
        break;
    case opcodes::binary:
    case opcodes::text:
        user_receive_callback();
        break;
    default:
        break;
    }
}

void ws::websocket::tcp_close_callback() {
    user_close_callback();
}

void ws::websocket::write_frame(std::span<uint8_t> data, opcodes opcode) {
    uint32_t masking_key;
    size_t olen;
    mbedtls_hardware_poll(nullptr, (uint8_t*)&masking_key, sizeof(masking_key), &olen);
    uint8_t frag_opcode = final_fragment | (uint8_t)opcode;
    uint8_t is_masked_length = masked;
    uint8_t length_field_size = (data.size() > 0xFFFF ? sizeof(uint64_t) : data.size() > 0x7D ? sizeof(uint16_t) : 0);
    uint8_t frame[2 + sizeof(masking_key) + length_field_size + data.size()];
    frame[0] = frag_opcode;
    uint8_t mask_offset = 2;
    if(data.size() < 0x7E) {
        is_masked_length |= (uint8_t)data.size();
    } else if(data.size() <= 0xFFFF) {
        is_masked_length |= has_length_16;
        uint16_t size = (uint16_t)data.size();
        memcpy(frame + 2, &size, sizeof(size));
        mask_offset += sizeof(size);
    } else {
        is_masked_length |= has_length_64;
        uint64_t size = (uint64_t)data.size();
        // this would pretty much fill up our memory if we tried to send a packet > 64kb...
        // probably shouldn't...
        memcpy(frame + 2, &size, sizeof(size));
        mask_offset += sizeof(size);
    }
    uint8_t data_offset = mask_offset + sizeof(masking_key);
    frame[1] = is_masked_length;
    memcpy(frame + mask_offset, &masking_key, sizeof(masking_key));
    mask(data, masking_key);
    memcpy(frame + data_offset, data.data(), data.size());
    tcp->write({frame, sizeof(frame)});
}