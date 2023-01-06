#pragma once

#include <string>
#include <functional>
#include <span>

#include <pico/cyw43_arch.h>

#include "lwip/pbuf.h"
#include "lwip/dns.h"
#include "lwip/tcp.h"

#include "circular_buffer.h"

#include "logger.h"

#define BUF_SIZE 2048
#define POLL_TIME_S 2

class tcp_client {
public:
    tcp_client(std::string addr, uint16_t port): port_(port), connected(false), sent_len(0), buffer_len(0), tcp_controlblock(nullptr), remote_addr({0}) {
        dns_init();
        ip_addr_t dnsserver;
        ip4addr_aton("1.1.1.1", &dnsserver);
        dns_setserver(0, &dnsserver);
        err_t err = dns_gethostbyname(addr.c_str(), &remote_addr, dns_callback, this);
        
        init();
        if(err == ERR_OK) {
            debug1("No dns lookup needed\n");
            connect();
        } else if(err != ERR_INPROGRESS) {
            error("gethostbyname failed with error code %d\n", err);
            close();
        }
    }

    bool init() {
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

    int available() {
        return buffer.size();
    }

    size_t read(std::span<uint8_t> &out) {
        return buffer.get(out);
    }

    bool write(std::span<const uint8_t> data) {
        cyw43_arch_lwip_begin();
        err_t err = tcp_write(tcp_controlblock, data.data(), data.size(), TCP_WRITE_FLAG_COPY);
        cyw43_arch_lwip_end();

        return err == ERR_OK;
    }

    bool connect() {
        cyw43_arch_lwip_begin();
        err_t err = tcp_connect(tcp_controlblock, &remote_addr, port_, connected_callback);
        cyw43_arch_lwip_end();

        return err == ERR_OK;
    }

    err_t close() {
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
        return err;
    }

private:
    struct tcp_pcb *tcp_controlblock;
    ip_addr_t remote_addr;
    circular_buffer<uint8_t> buffer{BUF_SIZE};
    int buffer_len;
    int sent_len;
    bool connected;
    uint16_t port_;

    static void dns_callback(const char* name, const ip_addr_t *addr, void* arg) {
        if(addr == nullptr) {
            error1("dns_callback: addr is nullptr\n");
            return;
        }
        info("ip of %s found: %d.%d.%d.%d\n", name, ip4_addr1(addr), ip4_addr2(addr), ip4_addr3(addr), ip4_addr4(addr));
        tcp_client *client = (tcp_client*)arg;
        client->remote_addr = *addr;
        client->connect();
    }

    static err_t poll_callback(void* arg, tcp_pcb* pcb) {
        tcp_client *client = (tcp_client*)arg;
        debug1("poll_callback\n");
        return ERR_OK;
    }

    static err_t sent_callback(void* arg, tcp_pcb* pcb, u16_t len) {
        tcp_client *client = (tcp_client*)arg;
        info("Sent %d bytes\n", len);
        return ERR_OK;
    }

    static err_t recv_callback(void* arg, tcp_pcb* pcb, pbuf* p, err_t err) {
        tcp_client *client = (tcp_client*)arg;
        info1("recv_callback\n");
        if(p == nullptr) {
            // Connection closed
            return client->close();
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

        return ERR_OK;
    }

    static void tcp_perror(err_t err) {
        switch(err) {
        case ERR_ABRT:
            printf("ERR_ABRT\n");
            break;
        case ERR_ALREADY:
            printf("ERR_ALREADY\n");
            break;
        case ERR_ARG:
            printf("ERR_ARG\n");
            break;
        case ERR_BUF:
            printf("ERR_BUF\n");
            break;
        case ERR_CLSD:
            printf("ERR_CLSD\n");
            break;
        case ERR_CONN:
            printf("ERR_CONN\n");
            break;
        case ERR_IF:
            printf("ERR_IF\n");
            break;
        case ERR_INPROGRESS:
            printf("ERR_INPROGRESS\n");
            break;
        case ERR_ISCONN:
            printf("ERR_ISCONN\n");
            break;
        case ERR_MEM:
            printf("ERR_MEM\n");
            break;
        case ERR_OK:
            printf("ERR_OK\n");
            break;
        case ERR_RST:
            printf("ERR_RST\n");
            break;
        case ERR_RTE:
            printf("ERR_RTE\n");
            break;
        case ERR_TIMEOUT:
            printf("ERR_TIMEOUT\n");
            break;
        case ERR_USE:
            printf("ERR_USE\n");
            break;
        case ERR_VAL:
            printf("ERR_VAL\n");
            break;
        case ERR_WOULDBLOCK:
            printf("ERR_WOULDBLOCK\n");
            break;
        }
    }

    static void err_callback(void* arg, err_t err) {
        tcp_client *client = (tcp_client*)arg;
        error1("TCP error: code ");
        tcp_perror(err);
        if (err != ERR_ABRT) {
            client->close();
        }
    }

    static err_t connected_callback(void* arg, tcp_pcb* pcb, err_t err) {
        tcp_client *client = (tcp_client*)arg;
        info1("connected_callback\n");
        if(err != ERR_OK) {
            error1("connect failed with error code ");
            tcp_perror(err);
            return client->close();
        }
        client->connected = true;
        return ERR_OK;
    }
};