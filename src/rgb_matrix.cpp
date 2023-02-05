#include "rgb_matrix.h"

#include "pico/multicore.h"
#include <cstring>

std::function<void()> main_loop = [](){};

template<uint rows, uint cols>
rgb_matrix<rows, cols>::rgb_matrix()
    : display_buffer(0)
    , sm_data(0)
    , sm_row(1)
    , pio(pio0)
    , has_flipped(false)
    , flip_on_vsync(false)
{
    main_loop = std::bind(&rgb_matrix<rows, cols>::run, this);
    data_prog_offs = pio_add_program(pio, &hub75e_data_program);
    row_prog_offs = pio_add_program(pio, &hub75e_row_program);
    hub75e_data_program_init(pio, sm_data, data_prog_offs, DATA_BASE_PIN, CLK_PIN);
    hub75e_row_program_init(pio, sm_row, row_prog_offs, ROWSEL_BASE_PIN, ROWSEL_N_PINS, LATCH_PIN);
}

template<uint rows, uint cols>
void rgb_matrix<rows, cols>::clear() {
    memset(&buffers[display_buffer][0], 0, rows * cols * sizeof(uint32_t));
    memset(&buffers[(display_buffer + 1) % 2][0], 0, rows * cols * sizeof(uint32_t));
}

template<uint rows, uint cols>
void rgb_matrix<rows, cols>::start() {
    multicore_reset_core1();
    multicore_launch_core1(&rgb_matrix::entry_point);
}

template<uint rows, uint cols>
void rgb_matrix<rows, cols>::run() {
    while (1) {
        for (int rowsel = 0; rowsel < (1 << ROWSEL_N_PINS); ++rowsel) {
            for (int bit = 0; bit < 8; ++bit) {
                hub75e_data_set_shift(pio, sm_data, data_prog_offs, bit);
                for (int x = 0; x < cols; ++x) {
                    pio_sm_put_blocking(pio, sm_data, buffers[display_buffer][rowsel * cols + x]);
                    pio_sm_put_blocking(pio, sm_data, buffers[display_buffer][((rows >> 1) + rowsel) * cols + x]);
                }
                // Dummy pixel per lane
                pio_sm_put_blocking(pio, sm_data, 0);
                pio_sm_put_blocking(pio, sm_data, 0);
                // SM is finished when it stalls on empty TX FIFO
                hub75e_wait_tx_stall(pio, sm_data);
                // Also check that previous OEn pulse is finished, else things can get out of sequence
                hub75e_wait_tx_stall(pio, sm_row);

                // Latch row data, pulse output enable for new row.
                pio_sm_put_blocking(pio, sm_row, rowsel | (100u * (1u << bit) << 5));
            }
        }
        if(flip_on_vsync) {
            display_buffer = (display_buffer + 1) % 2;
            flip_on_vsync = false;
            has_flipped = true;
        }
    }
}

template <uint rows, uint cols>
void rgb_matrix<rows, cols>::flip_buffer() {
    flip_on_vsync = true;
    has_flipped = false;
}

template <uint rows, uint cols>
bool rgb_matrix<rows, cols>::flipped() {
    return has_flipped;
}

template <uint rows, uint cols>
void rgb_matrix<rows, cols>::set_pixel(uint row, uint col, uint32_t color) {
    if(row >= rows || col >= cols) {
        return;
    }
    // Always write to the back buffer
    buffers[(display_buffer + 1) % 2][row * cols + col] = color;
}

template <uint rows, uint cols>
uint32_t rgb_matrix<rows, cols>::get_pixel(uint row, uint col) {
    if(row >= rows || col >= cols) {
        return 0;
    }
    // Always read from the front buffer
    return buffers[display_buffer][row * cols + col];
}

template <uint rows, uint cols>
void rgb_matrix<rows, cols>::set_pixel(uint row, uint col, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = r | ((uint32_t)g) << 8 | ((uint32_t)b) << 16;
    set_pixel(row, col, color);
}

template <uint rows, uint cols>
void rgb_matrix<rows, cols>::entry_point() {
    main_loop();
}