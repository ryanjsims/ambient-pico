#include "eio_client.h"
#include <charconv>
#include <cstring>
#include "hardware/watchdog.h"

class eio_packet {
public:
    eio_packet(): payload(14, ' ') {}

    eio_packet& operator+=(const std::string rhs) {
        payload += rhs;
        return *this;
    }

    eio_packet& operator+=(const char rhs) {
        payload += rhs;
        return *this;
    }

    std::span<uint8_t> span() {
        return {(uint8_t*)payload.data() + 14, payload.size() - 14};
    }

    const char* c_str() const noexcept {
        return payload.c_str();
    }

private:
    std::string payload;
};

eio_client::eio_client(ws::websocket *socket): socket_(socket), ping_milliseconds(0), open_(false) {
    trace1("eio_client (ctor)\n");
    socket_->on_receive(std::bind(&eio_client::ws_recv_callback, this));
    socket_->on_poll(1, std::bind(&eio_client::ws_poll_callback, this));
    socket_->on_closed(std::bind(&eio_client::ws_close_callback, this, std::placeholders::_1));
}

eio_client::eio_client(tcp_base *socket): ping_milliseconds(0), open_(false) {
    trace1("eio_client (ctor)\n");
    socket_ = new ws::websocket(socket);
    socket_->on_receive(std::bind(&eio_client::ws_recv_callback, this));
    socket_->on_poll(1, std::bind(&eio_client::ws_poll_callback, this));
    socket_->on_closed(std::bind(&eio_client::ws_close_callback, this, std::placeholders::_1));
}

size_t eio_client::read(std::span<uint8_t> data) {
    return socket_->read(data);
}

bool eio_client::send_message(std::span<uint8_t> data) {
    for(int i = -15; i < 0; i++) {
        if(data[i] != ' ') {
            error1("eio_client::send_message expects 15 extra space bytes before the beginning of the given span!\n");
            return false;
        }
    }
    data[-1] = (uint8_t)packet_type::message;
    debug("EIO send message: '%*s'\n", data.size() + 1, data.data() - 1);
    return socket_->write_text({data.data() - 1, data.size() + 1});
}

uint32_t eio_client::packet_size() const {
    return socket_->received_packet_size() - 1;
}

void eio_client::on_receive(std::function<void()> callback) {
    user_receive_callback = callback;
}

void eio_client::on_closed(std::function<void(err_t)> callback) {
    user_close_callback = callback;
}

void eio_client::on_open(std::function<void()> callback) {
    user_open_callback = callback;
}

void eio_client::read_initial_packet() {
    debug1("Engine reading initial packet...\n");
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
        open_ = true;
        user_open_callback();
        break;
    }

    case packet_type::close:
        debug1("EIO Close\n");
        open_ = false;
        socket_->close(ERR_CLSD);
        break;

    case packet_type::ping:{
        debug1("EIO Ping\n");
        ping_milliseconds = 0;
        eio_packet response;
        response += (char)packet_type::pong;
        socket_->write_text(response.span());
        break;
    }

    case packet_type::message:
        debug1("EIO Message\n");
        user_receive_callback();
        break;
    
    default:
        error("Unexpected packet type: %c\n", type);
        break;
    }
}

void eio_client::ws_poll_callback() {
    debug1("eio_client::ws_poll_callback\n");
    watchdog_update();
    if(open_) {
        ping_milliseconds += 1000;
        if(ping_milliseconds > ping_interval + ping_timeout) {
            open_ = false;
            socket_->close(ERR_TIMEOUT);
        }
    }
}

void eio_client::ws_close_callback(err_t reason) {
    user_close_callback(reason);
}