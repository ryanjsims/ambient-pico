#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/rand.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include "rgb_matrix.h"

struct coords {
    float x;
    float y;
};

class TrailMap {
public:
    TrailMap() {
        memset(data, 0, 4096);
    }

    uint32_t get(uint8_t x, uint8_t y) {
        return data[combine_pos(x, y)];
    }

    void add(uint8_t x, uint8_t y, uint32_t value) {
        data[combine_pos(x, y)] += value;
    }

    void decrement_all() {
        // std::vector<uint16_t> to_remove;
        for(int i = 0; i < 4096; i++) {
            uint8_t r, g, b;
            r = data[i] & 0xFF;
            g = (data[i] & 0xFF00) >> 8;
            b = (data[i] & 0xFF0000) >> 16;
            r = r * 0.9;
            g = g * 0.9;
            b = b * 0.9;
            data[i] = r | (((uint16_t)g) << 8) | (((uint32_t)b) << 16);
            // if(it->second == 0) {
            //     to_remove.push_back(it->first);
            // }
        }
        // for(auto it = to_remove.begin(); it != to_remove.end(); it++) {
        //     data.erase(*it);
        // }
    }
private:
    uint32_t data[4096];

    uint16_t combine_pos(uint8_t x, uint8_t y) {
        return ((uint16_t)y) * 64 + x;
    }
};

class Agent {
public:
    Agent(coords start, TrailMap* map)
        : position(start)
        , direction(((float)get_rand_32() / (float)UINT32_MAX) * M_TWOPI)
        , speed(lerp(0.5f, 1.5f, ((float)get_rand_32() / (float)UINT32_MAX)))
        , color(get_rand_32() & 0x00FFFFFF)
        , trails(map)
    {
        info("Created Agent with color %6X and direction %.2f\n", color, direction);
    }

    void update() {
        coords next_pos;
        next_pos.x = position.x + speed * cos(direction);
        next_pos.y = position.y + speed * sin(direction);
        if(next_pos.x > 63 || next_pos.y > 63 || next_pos.x < 0 || next_pos.y < 0) {
            direction = ((float)get_rand_32() / (float)UINT32_MAX) * M_TWOPI;
            next_pos.x = std::min(63.0f, std::max(0.0f, next_pos.x));
            next_pos.y = std::min(63.0f, std::max(0.0f, next_pos.y));
        }

        trails->add((uint8_t)roundf(position.x), (uint8_t)roundf(position.y), color);
        position = next_pos;
    }
private:
    coords position;
    float direction, speed;
    TrailMap* trails;
    uint32_t color;
};

int main() {
    stdio_init_all();
    sleep_ms(1000);
    rgb_matrix<64, 64> *matrix = new rgb_matrix<64, 64>();
    matrix->clear();
    matrix->flip_buffer();
    matrix->start();
    absolute_time_t frame_target = make_timeout_time_ms(16), start_time = get_absolute_time(), print_frametime_target = make_timeout_time_ms(1000);
    coords center{32, 32};
    uint8_t radius = 8;
    int direction = 1;
    bool create_frame = true;
    info1("Matrix started.\n");
    TrailMap* trails = new TrailMap();
    std::vector<Agent*> agents;
    for(int i = 0; i < 25; i++) {
        agents.push_back(new Agent(center, trails));
    }
    double smooth_frametime = 0.016;
    while(true) {
        if(create_frame && matrix->flipped()) {
            //info1("Creating frame...\n");

            for(uint8_t x = 0; x < 64; x++) {
                for(uint8_t y = 0; y < 64; y++) {
                    uint32_t pixel = trails->get(x, y);
                    matrix->set_pixel(y, x, pixel); 
                }
            }
            for(auto it = agents.begin(); it != agents.end(); it++) {
                (*it)->update();
            }
            create_frame = false;
        } else if(time_reached(frame_target) && matrix->flipped()) {
            //info1("Flipping buffer...\n");
            matrix->flip_buffer();
            absolute_time_t curr = get_absolute_time();
            double frame_time = ((double)absolute_time_diff_us(start_time, curr)) / 1000000.0;
            smooth_frametime = smooth_frametime + 0.5 * (frame_time - smooth_frametime);
            create_frame = true;
            frame_target = make_timeout_time_ms(16);
            start_time = curr;
            trails->decrement_all();
        }
        if(time_reached(print_frametime_target)) {
            info("%.2f fps\r", 1.0 / smooth_frametime);
            print_frametime_target = make_timeout_time_ms(1000);
        }
    }
}