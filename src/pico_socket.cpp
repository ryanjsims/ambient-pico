#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

#include "tcp_client.h"

void dump_bytes(const uint8_t *bptr, uint32_t len) {
    unsigned int i = 0;

    printf("dump_bytes %d", len);
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

int main() {
    stdio_init_all();
    cyw43_arch_init_with_country(CYW43_COUNTRY_USA);    

    cyw43_arch_enable_sta_mode();
    sleep_ms(1000);
    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect.\n");
        for(int i = 0; i < 3; i++) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            sleep_ms(100);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(250);
        }
        return 1;
    } else {
        printf("Connected.\n");
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    }

    tcp_client client("192.168.0.1", 80);

    uint8_t read_buf[512];
    std::span<uint8_t> data{read_buf, 512};
    while(true) {
        if(client.available()) {
            size_t count = client.read(data);
            dump_bytes(data.data(), count);
        }
    }
    cyw43_arch_deinit();
    return 0;
}