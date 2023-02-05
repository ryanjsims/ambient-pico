#pragma once

#include "logger.h"
#include "hardware/pio.h"
#include "hub75e.pio.h"
#include "math.h"

#include <functional>

#define DATA_BASE_PIN   0
#define DATA_N_PINS     6
#define ROWSEL_BASE_PIN 6
#define ROWSEL_N_PINS   5
#define CLK_PIN         11
#define LATCH_PIN       12
#define OEN_PIN         13

#include <limits.h>

static unsigned int int_log2 (unsigned int val) {
    if (val == 0) return UINT_MAX;
    if (val == 1) return 0;
    unsigned int ret = 0;
    while (val > 1) {
        val >>= 1;
        ret++;
    }
    return ret;
}

extern std::function<void()> main_loop;

template <uint rows, uint cols>
class rgb_matrix {
public:
    rgb_matrix();
    void clear();
    void start();
    void flip_buffer();
    bool flipped();
    void set_pixel(uint row, uint col, uint32_t color);
    void set_pixel(uint row, uint col, uint8_t r, uint8_t g, uint8_t b);
    uint32_t get_pixel(uint row, uint col);

private:
    uint32_t buffers[2][rows * cols];
    uint data_prog_offs, row_prog_offs, sm_data, sm_row;
    uint8_t display_buffer;
    volatile bool flip_on_vsync, has_flipped;
    PIO pio;

    void run();
    static void entry_point();
};

template class rgb_matrix<64u, 64u>;