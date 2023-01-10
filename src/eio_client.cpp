#include "eio_client.h"
#include <charconv>
#include <cstring>

eio_client::eio_client(ws::websocket *socket): socket_(socket), user_close_callback([](){}), user_receive_callback([](){}) {
    socket_->on_receive(std::bind(&eio_client::ws_recv_callback, this));
    socket_->on_closed(std::bind(&eio_client::ws_close_callback, this));
}

eio_client::eio_client(tcp_tls_client *socket): user_close_callback([](){}), user_receive_callback([](){}) {
    socket_ = new ws::websocket(socket);
    socket_->on_receive(std::bind(&eio_client::ws_recv_callback, this));
    socket_->on_closed(std::bind(&eio_client::ws_close_callback, this));
}

size_t eio_client::read(std::span<uint8_t> data) {
    return socket_->read(data);
}

void eio_client::send_message(std::span<uint8_t> data) {
    std::string packet;
    packet.resize(data.size() + 1);
    packet[0] = (uint8_t)packet_type::message;
    memcpy(packet.data() + 1, data.data(), data.size());
    socket_->write_text({(uint8_t*)packet.data(), packet.size()});
}

uint32_t eio_client::packet_size() const {
    return socket_->received_packet_size() - 1;
}

void eio_client::on_receive(std::function<void()> callback) {
    user_receive_callback = callback;
}

void eio_client::on_closed(std::function<void()> callback) {
    user_close_callback = callback;
}

void eio_client::read_initial_packet() {
    socket_->tcp_recv_callback();
}

void eio_client::ws_recv_callback() {
    packet_type type;
    socket_->read({(uint8_t*)&type, 1});
    switch(type) {
    case packet_type::open:{
        std::string packet;
        packet.resize(packet_size());
        socket_->read({(uint8_t*)packet.data(), packet.size()});
        size_t token_start = packet.find("sid") + 6;
        size_t token_end = packet.find("\"", token_start);
        sid = packet.substr(token_start, token_end - token_start);
        token_start = packet.find("pingInterval") + 14;
        token_end = packet.find_first_of(",}", token_start);
        std::from_chars(packet.c_str() + token_start, packet.c_str() + token_end, ping_interval);
        token_start = packet.find("pingTimeout") + 13;
        token_end = packet.find_first_of(",}", token_start);
        std::from_chars(packet.c_str() + token_start, packet.c_str() + token_end, ping_timeout);
        info("EIO Open:\n    sid=%s\n    pingInterval=%d\n    pingTimeout=%d\n", sid.c_str(), ping_interval, ping_timeout);
        break;
    }

    case packet_type::close:
        info1("EIO Close\n");
        ws_close_callback();
        break;

    case packet_type::ping:{
        info1("EIO Ping\n");
        packet_type response = packet_type::pong;
        socket_->write_text({(uint8_t*)&response, 1});
        break;
    }

    case packet_type::message:
        info1("EIO Message\n");
        user_receive_callback();
        break;
    
    default:
        error("Unexpected packet type: %c\n", type);
        break;
    }
}

void eio_client::ws_close_callback() {
    user_close_callback();
}