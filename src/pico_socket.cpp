#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <string_view>

#include "lwip/netif.h"

#define LOG_LEVEL LOG_LEVEL_DEBUG

#include "logger.h"
#include "tcp_client.h"
#include "http_client.h"

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
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    } else {
        printf("DOWN\n");
        for(int i = 0; i < 3; i++) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            sleep_ms(100);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(250);
        }
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
    int err;
    if (!(err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))) {
        info("Connected to %s\n", WIFI_SSID);
    }

    while(!netif_is_link_up(netif_default)){
        if(err) {
            error("failed to connect (code %d).\n", err);
            sleep_ms(1000);
            err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
        } 
    }

    info1("Connecting to ambientweather...\n");

    https_client client("rt2.ambientweather.net", 443);
    client.get("/socket.io/?EIO=4&transport=polling&api=1&applicationKey=" AMBIENT_WEATHER_APP_KEY);

    while(true) {
        sleep_ms(100);
        if(client.has_response()) {
            info("Got http response: %d %s\n", client.response().status(), client.response().get_status_text().c_str());
            info("Body:\n%s\n", client.response().get_body().c_str());
        }
    }
    cyw43_arch_deinit();
    return 0;
}