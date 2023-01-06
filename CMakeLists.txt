cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

include(lib/pico-sdk/pico_sdk_init.cmake)

project(ambient-pico)

pico_sdk_init()
add_executable(pico_socket src/pico_socket.cpp)
target_include_directories(pico_socket PRIVATE include)
target_link_libraries(pico_socket PRIVATE 
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
)
pico_add_extra_outputs(pico_socket)