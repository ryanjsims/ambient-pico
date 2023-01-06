#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

#include "lwip/pbuf.h"
#include "lwip/tcp.h"


int main() {
    setup_default_uart();
    cyw43_arch_init_with_country(CYW43_COUNTRY_USA);
    
    
    printf("Hello, world!\n");

    return 0;
}