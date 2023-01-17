#include "tcp_client.h"

#include <pico/cyw43_arch.h>

#include "lwip/pbuf.h"
#include "lwip/dns.h"
#include "lwip/tcp.h"

tcp_client::tcp_client()
    : port_(0)
    , connected_(false)
    , initialized_(false)
    , sent_len(0)
    , buffer_len(0)
    , tcp_controlblock(nullptr)
    , remote_addr({0})
    , user_receive_callback([](){})
    , user_connected_callback([](){})
    , user_closed_callback([](err_t){})
{
    info1("Initializing DNS...\n");
    dns_init();
    ip_addr_t dnsserver;
    ip4addr_aton("1.1.1.1", &dnsserver);
    dns_setserver(0, &dnsserver);
    
    info1("Initializing TCP Client\n");
    initialized_ = init();
}

bool tcp_client::init() {
    debug1("Initializing tcp_client\n");
    if(tcp_controlblock != nullptr) {
        error1("tcp_controlblock != null!\n");
        return false;
    }
    tcp_controlblock = tcp_new_ip_type(IPADDR_TYPE_V4);
    if(tcp_controlblock == nullptr) {
        error1("Failed to create tcp control block");
        return false;
    }
    tcp_arg(tcp_controlblock, this);
    tcp_poll(tcp_controlblock, poll_callback, POLL_TIME_S * 2);
    tcp_sent(tcp_controlblock, sent_callback);
    tcp_recv(tcp_controlblock, recv_callback);
    tcp_err(tcp_controlblock, err_callback);
    return true;
}

int tcp_client::available() const {
    return buffer.size();
}

size_t tcp_client::read(std::span<uint8_t> out) {
    return buffer.get(out);
}

bool tcp_client::write(std::span<const uint8_t> data) {
    cyw43_arch_lwip_begin();
    err_t err = tcp_write(tcp_controlblock, data.data(), data.size(), TCP_WRITE_FLAG_COPY);
    cyw43_arch_lwip_end();

    return err == ERR_OK;
}

bool tcp_client::connected() const {
    return connected_;
}

bool tcp_client::initialized() const {
    return initialized_;
}

bool tcp_client::connect(ip_addr_t addr, uint16_t port) {
    debug("tcp_client::connect to %s:%d\n", ip4addr_ntoa(&addr), port);
    remote_addr = addr;
    port_ = port;

    return connect();
}

bool tcp_client::connect(std::string addr, uint16_t port) {
    debug("tcp_client::connect to %s:%d\n", addr.c_str(), port);
    err_t err = dns_gethostbyname(addr.c_str(), &remote_addr, dns_callback, this);
    port_ = port;
    if(err == ERR_OK) {
        debug1("No dns lookup needed\n");
        err = connect();
    } else if(err != ERR_INPROGRESS) {
        error("gethostbyname failed with error code %d\n", err);
        close(err);
        return false;
    }

    return err == ERR_OK || err == ERR_INPROGRESS;
}

bool tcp_client::connect() {
    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(tcp_controlblock, &remote_addr, port_, connected_callback);
    cyw43_arch_lwip_end();

    return err == ERR_OK;
}

err_t tcp_client::close(err_t reason) {
    err_t err = ERR_OK;
    if (tcp_controlblock != NULL) {
        info1("Connection closing...\n");
        tcp_arg(tcp_controlblock, NULL);
        tcp_poll(tcp_controlblock, NULL, 0);
        tcp_sent(tcp_controlblock, NULL);
        tcp_recv(tcp_controlblock, NULL);
        tcp_err(tcp_controlblock, NULL);
        err = tcp_close(tcp_controlblock);
        if (err != ERR_OK) {
            error("close failed with code %d, calling abort\n", err);
            tcp_abort(tcp_controlblock);
            err = ERR_ABRT;
        }
        tcp_controlblock = NULL;
    }
    connected_ = false;
    initialized_ = false;
    user_closed_callback(reason);
    return err;
}

void tcp_client::on_poll(uint8_t interval_seconds, std::function<void()> callback) {
    tcp_poll(tcp_controlblock, poll_callback, interval_seconds * 2);
    user_poll_callback = callback;
}

void tcp_client::dns_callback(const char* name, const ip_addr_t *addr, void* arg) {
    if(addr == nullptr) {
        error1("dns_callback: addr is nullptr\n");
        return;
    }
    info("ip of %s found: %s\n", name, ipaddr_ntoa(addr));
    tcp_client *client = (tcp_client*)arg;
    client->remote_addr = *addr;
    client->connect();
}

err_t tcp_client::poll_callback(void* arg, tcp_pcb* pcb) {
    debug1("poll_callback\n");
    tcp_client *client = (tcp_client*)arg;
    client->user_poll_callback();
    return ERR_OK;
}

err_t tcp_client::sent_callback(void* arg, tcp_pcb* pcb, u16_t len) {
    tcp_client *client = (tcp_client*)arg;
    info("Sent %d bytes\n", len);
    return ERR_OK;
}

err_t tcp_client::recv_callback(void* arg, tcp_pcb* pcb, pbuf* p, err_t err) {
    tcp_client *client = (tcp_client*)arg;
    info1("recv_callback\n");
    if(p == nullptr) {
        // Connection closed
        return client->close(ERR_CLSD);
    }

    if(p->tot_len > 0) {
        info("recv'ing %d bytes\n", p->tot_len);
        // Receive the buffer
        size_t count = 0;
        pbuf* curr = p;
        while(curr && !client->buffer.full()) {
            count += client->buffer.put({reinterpret_cast<uint8_t*>(curr->payload), curr->len});
            curr = curr->next;
        }
        tcp_recved(pcb, count);
    }
    pbuf_free(p);

    client->user_receive_callback();

    return ERR_OK;
}

void tcp_client::tcp_perror(err_t err) {
    switch(err) {
    case ERR_ABRT:
        error_cont1("ERR_ABRT\n");
        break;
    case ERR_ALREADY:
        error_cont1("ERR_ALREADY\n");
        break;
    case ERR_ARG:
        error_cont1("ERR_ARG\n");
        break;
    case ERR_BUF:
        error_cont1("ERR_BUF\n");
        break;
    case ERR_CLSD:
        error_cont1("ERR_CLSD\n");
        break;
    case ERR_CONN:
        error_cont1("ERR_CONN\n");
        break;
    case ERR_IF:
        error_cont1("ERR_IF\n");
        break;
    case ERR_INPROGRESS:
        error_cont1("ERR_INPROGRESS\n");
        break;
    case ERR_ISCONN:
        error_cont1("ERR_ISCONN\n");
        break;
    case ERR_MEM:
        error_cont1("ERR_MEM\n");
        break;
    case ERR_OK:
        error_cont1("ERR_OK\n");
        break;
    case ERR_RST:
        error_cont1("ERR_RST\n");
        break;
    case ERR_RTE:
        error_cont1("ERR_RTE\n");
        break;
    case ERR_TIMEOUT:
        error_cont1("ERR_TIMEOUT\n");
        break;
    case ERR_USE:
        error_cont1("ERR_USE\n");
        break;
    case ERR_VAL:
        error_cont1("ERR_VAL\n");
        break;
    case ERR_WOULDBLOCK:
        error_cont1("ERR_WOULDBLOCK\n");
        break;
    }
}

void tcp_client::err_callback(void* arg, err_t err) {
    tcp_client *client = (tcp_client*)arg;
    error1("TCP error: code ");
    tcp_perror(err);
    if (err != ERR_ABRT) {
        client->close(err);
    }
}

err_t tcp_client::connected_callback(void* arg, tcp_pcb* pcb, err_t err) {
    tcp_client *client = (tcp_client*)arg;
    info1("tcp_client::connected_callback\n");
    if(err != ERR_OK) {
        error1("connect failed with error code ");
        tcp_perror(err);
        return client->close(err);
    }
    client->connected_ = true;
    client->user_connected_callback();
    return ERR_OK;
}