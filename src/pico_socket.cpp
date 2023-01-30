#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <pico/cyw43_arch.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <string_view>
#include "nlohmann/json.hpp"

#include "lwip/netif.h"

#include "logger.h"
#include "sio_client.h"
#include "max7219.h"

#define RED_GPIO   13
#define GREEN_GPIO 14
#define BLUE_GPIO  15

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
        pwm_set_gpio_level(BLUE_GPIO, 0xFFFFu);
        pwm_set_gpio_level(RED_GPIO, 0xFFFFu);
        pwm_set_gpio_level(GREEN_GPIO, 0x8000u);
    } else {
        info_cont1("DOWN\n");
        pwm_set_gpio_level(BLUE_GPIO, 0xFFFFu);
        pwm_set_gpio_level(RED_GPIO, 0x8000u);
        pwm_set_gpio_level(GREEN_GPIO, 0xFFFFu);
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

uint8_t wifi_forward_anim[9] = {
    0b01000000,
    0b00100000,
    0b00000001,
    0b00000100,
    0b00001000,
    0b00010000,
    0b00000001,
    0b00000010,
    0b01000000
};

uint8_t wifi_backward_anim[9] = {
    0b00001000,
    0b00000100,
    0b00000001,
    0b00100000,
    0b01000000,
    0b00000010,
    0b00000001,
    0b00010000,
    0b00001000
};

uint8_t wifi_forward_transition[2] = {
    0b00100000,
    0b00010000
};

uint8_t wifi_backward_transition[2] = {
    0b00000100,
    0b00000010
};

void draw_frame(int idx) {
    uint8_t *data;
    uint8_t mod, digit;
    if(idx < 36) {
        data = wifi_forward_anim;
        mod = 9;
        digit = MAX7219_REG_DIGIT3 - (int)(idx / 9);
    } else if(idx < 38) {
        data = wifi_forward_transition;
        mod = 2;
        digit = MAX7219_REG_DIGIT0;
    } else if(idx < 74) {
        data = wifi_backward_anim;
        mod = 9;
        digit = (int)((idx - 38) / 9) + 1;
    } else if(idx < 76) {
        data = wifi_backward_transition;
        mod = 2;
        digit = MAX7219_REG_DIGIT3;
    }
    max7219_write_reg(MAX7219_REG_DIGIT0, 0);
    max7219_write_reg(MAX7219_REG_DIGIT1, 0);
    max7219_write_reg(MAX7219_REG_DIGIT2, 0);
    max7219_write_reg(MAX7219_REG_DIGIT3, 0);
    max7219_write_reg(digit, data[idx % mod]);
}

bool stop_anim = false;

void run_anim() {
    max7219_write_reg(MAX7219_REG_DIGIT7, 0b01001110); // C
    max7219_write_reg(MAX7219_REG_DIGIT6, 0b00011101); // o
    max7219_write_reg(MAX7219_REG_DIGIT5, 0b00010101); // n
    max7219_write_reg(MAX7219_REG_DIGIT4, 0b00010101); // n
    int frame = 0;
    absolute_time_t next_frame = make_timeout_time_ms(20);
    while(!stop_anim) {
        if(frame == 76) {
            frame = 0;
        }
        if(time_reached(next_frame)) {
            draw_frame(frame++);
            next_frame = make_timeout_time_ms(20);
        }
        sleep_ms(5);
    }
}

int main() {
    stdio_init_all();
    sleep_ms(10);

    if(watchdog_caused_reboot()) {
        for(int i = 0; i < 3; i++) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            sleep_ms(250);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(250);
        }
    }
    
    max7219_init();
    multicore_reset_core1();
    multicore_launch_core1(run_anim);

    for(uint i = RED_GPIO; i <= BLUE_GPIO; i++) {
        gpio_set_function(i, GPIO_FUNC_PWM);
        uint slice_num = pwm_gpio_to_slice_num(i);

        pwm_config config = pwm_get_default_config();
        // Set divider, reduces counter clock to sysclock/this value
        pwm_config_set_clkdiv(&config, 4.f);
        // Load the configuration into our PWM slice, and set it running.
        pwm_init(slice_num, &config, false);
        pwm_set_gpio_level(i, 0xffffu);
    }
    pwm_set_mask_enabled((1 << pwm_gpio_to_slice_num(RED_GPIO)) | (1 << pwm_gpio_to_slice_num(GREEN_GPIO)) | (1 << pwm_gpio_to_slice_num(BLUE_GPIO)));
    pwm_set_gpio_level(BLUE_GPIO, 0xFFFFu);
    pwm_set_gpio_level(RED_GPIO, 0xFFFFu);
    pwm_set_gpio_level(GREEN_GPIO, 0xFFFFu);

    cyw43_arch_init_with_country(CYW43_COUNTRY_USA);    

    cyw43_arch_enable_sta_mode();
    info1("Connecting to WiFi...\n");
    pwm_set_gpio_level(BLUE_GPIO, 0x8000u);
    pwm_set_gpio_level(RED_GPIO, 0xFFFFu);
    pwm_set_gpio_level(GREEN_GPIO, 0xFFFFu);
    netif_set_status_callback(netif_default, netif_status_callback);
    int err = cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
    int status = CYW43_LINK_UP + 1;

    while(!(netif_is_link_up(netif_default) && netif_ip4_addr(netif_default)->addr != 0)){
        if(err || status < 0) {
            if(err) {
                error("failed to start wifi scan (code %d).\n", err);
            } else {
                error("failed to join network (code %d).\n", status);
            }
            sleep_ms(1000);
            err = cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
            status = CYW43_LINK_UP + 1;
            continue;
        } 
        int new_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        if(status != new_status) {
            debug("Wifi status change: %d -> %d\n", status, new_status);
            status = new_status;
        }
    }

    info1("Connecting to ambientweather...\n");
    const ip4_addr_t *address = netif_ip4_addr(netif_default);
    info("My IP Address is %d.%d.%d.%d\n", ip4_addr1(address), ip4_addr2(address), ip4_addr3(address), ip4_addr4(address));
    sio_client client("https://rt2.ambientweather.net/", {{"api", "1"}, {"applicationKey", AMBIENT_WEATHER_APP_KEY}});
    //sio_client client("http://192.168.0.13:8000/", {{"api", "1"}, {"applicationKey", AMBIENT_WEATHER_APP_KEY}});

    
    client.on_open([&client](){
        info1("User open callback\n");
        client.connect();
    });
    debug1("set client open handler\n");
    
    client.socket()->on("subscribed", [](nlohmann::json body){
        info("Subscribed:\n%s\n", body[0].dump(4).c_str());
        stop_anim = true;
        // Wait for animation to end
        sleep_us(5100);
        max7219_ensure_init();
        max7219_write(body[0]["devices"][0]["lastData"]["tempf"]);
        max7219_write(body[0]["devices"][0]["lastData"]["temp1f"], 4);
    });
    debug1("set socket subscribed handler\n");

    client.socket()->on("data", [](nlohmann::json body){
        info("Data:\n%s\n", body[0].dump(4).c_str());
        max7219_write(body[0]["tempf"]);
        max7219_write(body[0]["temp1f"], 4);
    });
    debug1("set socket data handler\n");

    client.socket()->on("connect", [&client](nlohmann::json args){
        debug1("connect handler called\n");
        pwm_set_gpio_level(BLUE_GPIO, 0x8000u);
        pwm_set_gpio_level(RED_GPIO, 0xFFFFu);
        pwm_set_gpio_level(GREEN_GPIO, 0x8000u);
        nlohmann::json object;
        info1("Emitting subscribe event\n");
        object["apiKeys"] = {AMBIENT_WEATHER_API_KEY};
        client.socket()->emit("subscribe", object);
    });
    debug1("set socket connect handler\n");

    client.socket()->on("disconnect", [&](nlohmann::json args){
        std::string reason = args[0];
        info("Disconnected: '%s'\n", reason.c_str());
        pwm_set_gpio_level(BLUE_GPIO, 0x8000u);
        pwm_set_gpio_level(RED_GPIO, 0x8000u);
        pwm_set_gpio_level(GREEN_GPIO, 0xffffu);
        if(reason == "io server disconnect") {
            client.connect();
        }
    });
    debug1("set socket disconnect handler\n");
    
    client.run();
    cyw43_arch_deinit();
    return 0;
}