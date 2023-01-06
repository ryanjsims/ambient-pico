#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <string_view>

#include "lwip/netif.h"

#include "logger.h"
#include "tcp_client.h"

using namespace std::string_view_literals;

void dump_bytes(const uint8_t *bptr, uint32_t len) {
    unsigned int i = 0;

    info("dump_bytes %d", len);
    for (i = 0; i < len;) {
        if ((i & 0x0f) == 0) {
            printf("\n");
        } else if ((i & 0x07) == 0) {
            printf(" ");
        }
        printf("%02x ", bptr[i++]);
    }
    printf("\n");
}

void netif_status_callback(netif* netif) {
    info1("netif status change:\n");
    info1("    Link ");
    if(netif_is_link_up(netif)) {
        printf("UP\n");
    } else {
        printf("DOWN\n");
    }
    const ip4_addr_t *address = netif_ip4_addr(netif);
    info1("    IP Addr: ");
    if(address) {
        printf("%d.%d.%d.%d\n", ip4_addr1(address), ip4_addr2(address), ip4_addr3(address), ip4_addr4(address));
    } else {
        printf("(null)\n");
    }
}

int main() {
    stdio_init_all();
    cyw43_arch_init_with_country(CYW43_COUNTRY_USA);    

    cyw43_arch_enable_sta_mode();
    sleep_ms(1000);
    info1("Connecting to WiFi...\n");
    netif_set_status_callback(netif_default, netif_status_callback);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        error1("failed to connect.\n");
        for(int i = 0; i < 3; i++) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            sleep_ms(100);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(250);
        }
        return 1;
    } else {
        info("Connected to %s\n", WIFI_SSID);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    }

    tcp_client client("192.168.0.11", 9999);

    std::string_view view = "test"sv;
    client.write({reinterpret_cast<const uint8_t*>(view.data()), view.size()});

    uint8_t read_buf[512];
    std::span<uint8_t> data{read_buf, 512};
    while(true) {
        if(client.available()) {
            size_t count = client.read(data);
            dump_bytes(data.data(), count);
            for(int i = 0; i < count; i++){
                printf("%c", data[i]);
            }
            printf("\n");
        }
    }
    cyw43_arch_deinit();
    return 0;
}