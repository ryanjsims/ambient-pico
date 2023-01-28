#include "websocket.h"

#include "lwip/ip_addr.h"

ws::websocket::websocket(tcp_base *socket): tcp(socket), user_receive_callback([](){}), user_close_callback([](err_t){}) {
    tcp->on_receive(std::bind(&websocket::tcp_recv_callback, this));
    tcp->on_closed(std::bind(&websocket::tcp_close_callback, this, std::placeholders::_1));
}

ws::websocket::~websocket() {
    delete tcp;
}

bool ws::websocket::write_text(std::span<uint8_t> data) {
    return write_frame(data, opcodes::text);
}

bool ws::websocket::write_binary(std::span<uint8_t> data) {
    return write_frame(data, opcodes::binary);
}

void ws::websocket::close(err_t reason) {
    tcp->close(reason);
}

bool ws::websocket::connected() {
    return tcp->connected();
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

void ws::websocket::on_poll(uint8_t interval_seconds, std::function<void()> callback) {
    tcp->on_poll(interval_seconds, std::bind(&websocket::tcp_poll_callback, this));
    user_poll_callback = callback;
}

void ws::websocket::on_closed(std::function<void(err_t)> callback) {
    user_close_callback = callback;
}

void ws::websocket::mask(std::span<uint8_t> data, uint32_t masking_key) {
    std::span<uint8_t> masking_bytes = {(uint8_t*)&masking_key, sizeof(masking_key)};
    for(uint32_t i = 0; i < data.size(); i++) {
        data[i] ^= masking_bytes[i % 4];
    }
}

void ws::websocket::tcp_recv_callback() {
    if(!tcp->available()) {
        return;
    }
    uint8_t frame_header[2];
    tcp->read({frame_header, 2});
    packet_size = frame_header[1] & 0x7F;
    debug("Header: %02x %02x\n", frame_header[0], frame_header[1]);
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
    switch(opcodes(frame_header[0] & 0x0F)) {
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
    case opcodes::close:{
        debug1("ws::websocket::tcp_recv_callback: Got close frame\n");
        std::string data(' ', 14);
        write_frame({(uint8_t*)data.data() + 14, data.size() - 14}, opcodes::close);
        tcp->close(ERR_CLSD);
        break;
    }
    default:
        break;
    }
}

void ws::websocket::tcp_poll_callback() {
    user_poll_callback();
}

void ws::websocket::tcp_close_callback(err_t reason) {
    user_close_callback(reason);
}

#define htonll(x) ((((x) & (u64_t)0x00000000000000ffULL) << 56) | \
                   (((x) & (u64_t)0x000000000000ff00ULL) << 40) | \
                   (((x) & (u64_t)0x0000000000ff0000ULL) << 24) | \
                   (((x) & (u64_t)0x00000000ff000000ULL) <<  8) | \
                   (((x) & (u64_t)0x000000ff00000000ULL) >>  8) | \
                   (((x) & (u64_t)0x0000ff0000000000ULL) >> 24) | \
                   (((x) & (u64_t)0x00ff000000000000ULL) >> 40) | \
                   (((x) & (u64_t)0xff00000000000000ULL) >> 56))

bool ws::websocket::write_frame(std::span<uint8_t> data, opcodes opcode) {
    for(int i = -14; i < 0; i++) {
        if(data[i] != ' ') {
            error1("ws::websocket::write_frame expects 14 extra space bytes before the beginning of the given span!\n");
            return false;
        }
    }
    debug("websocket::write data=%p size=%d\n", data.data(), data.size());
    uint32_t masking_key;
    size_t olen;
    mbedtls_hardware_poll(nullptr, (uint8_t*)&masking_key, sizeof(masking_key), &olen);
    uint8_t frag_opcode = final_fragment | (uint8_t)opcode;
    uint8_t is_masked_length = masked;
    uint8_t length_field_size = (data.size() > 0xFFFF ? sizeof(uint64_t) : data.size() > 0x7D ? sizeof(uint16_t) : 0);
    //uint8_t frame[2 + sizeof(masking_key) + length_field_size + data.size()];
    uint8_t mask_offset = 2;
    uint16_t length16;
    uint64_t length64;
    if(data.size() < 0x7E) {
        is_masked_length |= (uint8_t)data.size();
    } else if(data.size() <= 0xFFFF) {
        is_masked_length |= has_length_16;
        length16 = htons((uint16_t)data.size());
        //memcpy(frame + 2, &size, sizeof(size));
        mask_offset += sizeof(length16);
    } else {
        is_masked_length |= has_length_64;
        length64 = htonll((uint64_t)data.size());
        // this would pretty much fill up our memory if we tried to send a packet > 64kb...
        // probably shouldn't...
        //memcpy(frame + 2, &size, sizeof(size));
        mask_offset += sizeof(length64);
    }
    uint8_t data_offset = mask_offset + sizeof(masking_key);
    data[-data_offset] = frag_opcode;
    data[-data_offset + 1] = is_masked_length;
    if((is_masked_length & 0x7F) == has_length_16) {
        memcpy(data.data() -data_offset + 2, &length16, sizeof(length16));
    } else if((is_masked_length & 0x7F) == has_length_64) {
        memcpy(data.data() -data_offset + 2, &length64, sizeof(length64));
    }
    memcpy(data.data() - data_offset + mask_offset, &masking_key, sizeof(masking_key));
    mask(data, masking_key);
    bool res = tcp->write({data.data() - data_offset, data.size() + data_offset});
    debug("tcp->write result: %d\n", res);
    return res;
}