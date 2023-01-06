#pragma once

#include <string>
#include <functional>
#include <span>

#include <pico/cyw43_arch.h>

#include "lwip/pbuf.h"
#include "lwip/dns.h"
#include "lwip/tcp.h"

#include "circular_buffer.h"

#define BUF_SIZE 2048
#define POLL_TIME_S 2

class tcp_client {
public:
    tcp_client(std::string addr, uint16_t port): port_(port), connected(false), sent_len(0), buffer_len(0), tcp_controlblock(tcp_new_ip_type(IPADDR_TYPE_V4)), remote_addr({0}) {
        if(tcp_controlblock == nullptr) {
            printf("Failed to create tcp control block");
        } else {
            dns_init();
            ip_addr_t dnsserver;
            ip4addr_aton("1.1.1.1", &dnsserver);
            dns_setserver(0, &dnsserver);
            err_t err = dns_gethostbyname(addr.c_str(), &remote_addr, dns_callback, this);
            
            tcp_arg(tcp_controlblock, this);
            tcp_poll(tcp_controlblock, poll_callback, POLL_TIME_S * 2);
            tcp_sent(tcp_controlblock, sent_callback);
            tcp_recv(tcp_controlblock, recv_callback);
            tcp_err(tcp_controlblock, err_callback);

            if(err == ERR_OK) {
                printf("No dns lookup needed\n");
                connect();
            } else if(err != ERR_INPROGRESS) {
                printf("gethostbyname failed with error code %d\n", err);
                close();
            }
        }
    }

    int available() {
        return buffer.size();
    }

    size_t read(std::span<uint8_t> &out) {
        return buffer.get(out);
    }

    bool write(std::span<uint8_t> data) {
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
            tcp_arg(tcp_controlblock, NULL);
            tcp_poll(tcp_controlblock, NULL, 0);
            tcp_sent(tcp_controlblock, NULL);
            tcp_recv(tcp_controlblock, NULL);
            tcp_err(tcp_controlblock, NULL);
            err = tcp_close(tcp_controlblock);
            if (err != ERR_OK) {
                printf("close failed with code %d, calling abort\n", err);
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
            printf("dns_callback: addr is nullptr\n");
            return;
        }
        printf("ip of %s found: %d.%d.%d.%d\n", name, ip4_addr1(addr), ip4_addr2(addr), ip4_addr3(addr), ip4_addr4(addr));
        tcp_client *client = (tcp_client*)arg;
        client->remote_addr = *addr;
        client->connect();
    }

    static err_t poll_callback(void* arg, tcp_pcb* pcb) {
        tcp_client *client = (tcp_client*)arg;
        //printf("poll_callback\n");
        return ERR_OK;
    }

    static err_t sent_callback(void* arg, tcp_pcb* pcb, u16_t len) {
        tcp_client *client = (tcp_client*)arg;
        printf("Sent %d bytes\n", len);
        return ERR_OK;
    }

    static err_t recv_callback(void* arg, tcp_pcb* pcb, pbuf* p, err_t err) {
        tcp_client *client = (tcp_client*)arg;
        printf("recv_callback\n");
        if(p == nullptr) {
            // Connection closed
            return client->close();
        }

        if(p->tot_len > 0) {
            printf("recv'ing %d bytes\n", p->tot_len);
            // Receive the buffer
            size_t count = 0;
            pbuf* curr = p, *next;
            while(curr && !client->buffer.full()) {
                count += client->buffer.put({reinterpret_cast<uint8_t*>(curr->payload), curr->len});
                next = pbuf_dechain(curr);
                pbuf_free(curr);
                curr = next;
            }
            tcp_recved(pcb, count);
            p = curr;
        }
        if(p != NULL)
            pbuf_free(p);

        return ERR_OK;
    }

    static void err_callback(void* arg, err_t err) {
        tcp_client *client = (tcp_client*)arg;
        printf("TCP error: code %d\n", err);
        if (err != ERR_ABRT) {
            client->close();
        }
    }

    static err_t connected_callback(void* arg, tcp_pcb* pcb, err_t err) {
        tcp_client *client = (tcp_client*)arg;
        printf("connected_callback\n");
        if(err != ERR_OK) {
            printf("connect failed with error code %d\n", err);
            return client->close();
        }
        client->connected = true;
        return ERR_OK;
    }
};