#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <string_view>
#include "nlohmann/json.hpp"

#include "lwip/netif.h"

//#define LOG_LEVEL LOG_LEVEL_INFO

#include "logger.h"
#include "tcp_client.h"
#include "http_client.h"
#include "websocket.h"
#include "eio_client.h"
#include "sio_client.h"

using namespace std::string_view_literals;


void dump_bytes(const uint8_t *bptr, uint32_t len) {
    unsigned int i = 0;

    info("dump_bytes %d", len);
    for (i = 0; i < len;) {
        if ((i & 0x0f) == 0) {
            info_cont1("\n");
        } else if ((i & 0x07) == 0) {
            info_cont1(" ");
        }
        info_cont("%02x ", bptr[i++]);
    }
    info_cont1("\n");
}

void netif_status_callback(netif* netif) {
    info1("netif status change:\n");
    info1("    Link ");
    if(netif_is_link_up(netif)) {
        info_cont1("UP\n");
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    } else {
        info_cont1("DOWN\n");
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
        info_cont("%d.%d.%d.%d\n", ip4_addr1(address), ip4_addr2(address), ip4_addr3(address), ip4_addr4(address));
    } else {
        info_cont1("(null)\n");
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
    sio_client client("https://rt2.ambientweather.net/", {{"api", "1"}, {"applicationKey", AMBIENT_WEATHER_APP_KEY}});
    client.socket()->on("subscribed", [](nlohmann::json body){
        info("Subscribed:\n%s\n", body[0].dump(4).c_str());
    });

    client.socket()->on("data", [](nlohmann::json body){
        info("Data:\n%s\n", body[0].dump(4).c_str());
    });

    client.socket()->once("connect", [&client](nlohmann::json body){
        info1("connect handler called\n");
    });

    int state = 0;

    while(true) {
        sleep_ms(100);
        if(state == 0 && client.ready()) {
            client.connect();
            state = 1;
        } else if(state == 1 && client.socket()->connected()) {
            info1("Emitting subscribe event\n");
            nlohmann::json object;
            object["apiKeys"] = {AMBIENT_WEATHER_API_KEY};
            client.socket()->emit("subscribe", object);
            state = 2;
        }
    }
    cyw43_arch_deinit();
    return 0;
}