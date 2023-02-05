cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

include(lib/pico-sdk/pico_sdk_init.cmake)

project(ambient-pico)

pico_sdk_init()
add_executable(pico_socket 
    src/pico_socket.cpp
    src/tcp_client.cpp
    src/circular_buffer.cpp
    src/tcp_tls_client.cpp
    src/http_client.cpp
    src/websocket.cpp
    src/eio_client.cpp
    src/sio_client.cpp
    src/LUrlParser.cpp
    src/max7219.cpp
    src/rgb_matrix.cpp
)

pico_generate_pio_header(pico_socket ${CMAKE_CURRENT_LIST_DIR}/src/pio/hub75e.pio)

target_include_directories(pico_socket PRIVATE include lib/json/single_include)
target_link_libraries(pico_socket PRIVATE 
    pico_stdlib
    pico_mem_ops
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mbedtls
    pico_mbedtls
    pico_multicore
    hardware_pwm
    hardware_spi
    hardware_dma
    hardware_pio
)
target_compile_options(pico_socket PRIVATE "-Wno-psabi")
target_compile_definitions(pico_socket PRIVATE "WIFI_SSID=\"$ENV{WIFI_SSID}\"" "WIFI_PASSWORD=\"$ENV{WIFI_PASS}\"" "AMBIENT_WEATHER_APP_KEY=\"$ENV{AMBIENT_WEATHER_APP_KEY}\"" "AMBIENT_WEATHER_API_KEY=\"$ENV{AMBIENT_WEATHER_API_KEY}\"")
pico_enable_stdio_usb(pico_socket 1)
pico_enable_stdio_uart(pico_socket 0)
pico_add_extra_outputs(pico_socket)

add_executable(pico_matrix
    src/pico_matrix.cpp
    src/rgb_matrix.cpp
)

pico_generate_pio_header(pico_matrix ${CMAKE_CURRENT_LIST_DIR}/src/pio/hub75e.pio)
target_include_directories(pico_matrix PRIVATE include)
target_link_libraries(pico_matrix PRIVATE
    pico_stdlib
    pico_mem_ops
    pico_multicore
    hardware_dma
    hardware_pio
    hardware_pwm
)
target_compile_options(pico_matrix PRIVATE "-Wno-psabi")
target_compile_definitions(pico_matrix PRIVATE "WIFI_SSID=\"$ENV{WIFI_SSID}\"" "WIFI_PASSWORD=\"$ENV{WIFI_PASS}\"" "AMBIENT_WEATHER_APP_KEY=\"$ENV{AMBIENT_WEATHER_APP_KEY}\"" "AMBIENT_WEATHER_API_KEY=\"$ENV{AMBIENT_WEATHER_API_KEY}\"")
pico_enable_stdio_usb(pico_matrix 1)
pico_enable_stdio_uart(pico_matrix 0)
pico_add_extra_outputs(pico_matrix)

add_subdirectory(lib/pico-examples/pio/hub75)