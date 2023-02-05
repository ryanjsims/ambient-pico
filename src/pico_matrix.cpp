#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <time.h>

#include "rgb_matrix.h"

struct coords {
    uint16_t x;
    uint16_t y;
};

int main() {
    stdio_init_all();
    sleep_ms(1000);
    rgb_matrix<64, 64> *matrix = new rgb_matrix<64, 64>();
    matrix->clear();
    matrix->flip_buffer();
    matrix->start();
    absolute_time_t frame_target = make_timeout_time_ms(16);
    coords center{32, 32};
    uint8_t radius = 8;
    int direction = 1;
    bool create_frame = true;
    info1("Matrix started.\n");
    while(true) {
        if(create_frame && matrix->flipped()) {
            //info1("Creating frame...\n");
            for(int16_t x = 0; x < 64; x++) {
                uint16_t x_dist_sq = (x - center.x) * (x - center.x);
                for(int16_t y = 0; y < 64; y++) {
                    uint16_t y_dist_sq = (y - center.y) * (y - center.y);
                    int16_t distance = sqrt(x_dist_sq + y_dist_sq) - radius;
                    uint32_t pixel = matrix->get_pixel(y, x) & 0xFFFFFF00;
                    matrix->set_pixel(y, x, pixel | (distance > 0 ? distance & 0xFF : 0)); 
                }
            }
            
            create_frame = false;
        } else if(time_reached(frame_target) && matrix->flipped()) {
            //info1("Flipping buffer...\n");
            matrix->flip_buffer();
            create_frame = true;
            frame_target = make_timeout_time_ms(16);
            // center.x = (center.x + direction);
            // if(center.x == 63) {
            //     direction = -1;
            // } else if(center.x == 0) {
            //     direction = 1;
            // }
            center.x = 32 + 12 * cos(3.14 * ((float)to_ms_since_boot(get_absolute_time())) / 250);
            center.y = 32 + 12 * sin(3.14 * ((float)to_ms_since_boot(get_absolute_time())) / 250);
        }
    }
}