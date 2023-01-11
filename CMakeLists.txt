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
)
target_include_directories(pico_socket PRIVATE include lib/json/single_include)
target_link_libraries(pico_socket PRIVATE 
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mbedtls
    pico_mbedtls
)
target_compile_options(pico_socket PRIVATE "-Wno-psabi")
target_compile_definitions(pico_socket PRIVATE "WIFI_SSID=\"$ENV{WIFI_SSID}\"" "WIFI_PASSWORD=\"$ENV{WIFI_PASS}\"" "AMBIENT_WEATHER_APP_KEY=\"$ENV{AMBIENT_WEATHER_APP_KEY}\"" "AMBIENT_WEATHER_API_KEY=\"$ENV{AMBIENT_WEATHER_API_KEY}\"")
pico_enable_stdio_usb(pico_socket 1)
pico_enable_stdio_uart(pico_socket 0)
pico_add_extra_outputs(pico_socket)