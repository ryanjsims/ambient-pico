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

uint state = get_rand_32();
uint hash()
{
    state ^= 2747636419u;
    state *= 2654435769u;
    state ^= state >> 16;
    state *= 2654435769u;
    state ^= state >> 16;
    state *= 2654435769u;
    return state;
}


class TrailMap {
public:
    TrailMap() {
        memset(data, 0, 4096 * sizeof(uint32_t));
    }

    uint32_t get(uint8_t x, uint8_t y) {
        if(x > 63 || y > 63) {
            return 0;
        }
        return data[combine_pos(x, y)];
    }

    void add(uint8_t x, uint8_t y, uint32_t value) {
        data[combine_pos(x, y)] += value;
    }

    void decrement_all() {
        // std::vector<uint16_t> to_remove;
        for(int i = 0; i < 4096; i++) {
            if(data[i] == 0) {
                continue;
            }
            uint8_t r, g, b;
            r = data[i] & 0xFF;
            g = (data[i] & 0xFF00) >> 8;
            b = (data[i] & 0xFF0000) >> 16;
            r = r * 0.875f;
            g = g * 0.875f;
            b = b * 0.875f;
            data[i] = r; 
            data[i] |= (((uint16_t)g) << 8);
            data[i] |= (((uint32_t)b) << 16);
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

const float sense_offset = 2, sensor_angle_rad = M_PI_4, turnSpeed = 0.1f * 120 * M_PI;

class Agent {
public:
    Agent(coords start, TrailMap* map)
        : position(start)
        , direction(((float)hash() / (float)UINT32_MAX) * M_TWOPI)
        , speed(20.0f * lerp(0.5f, 1.2f, ((float)hash() / (float)UINT32_MAX)))
        , color(hash() & 0x003F3F3F)
        , trails(map)
    {
        // uint8_t r = color, g = color >> 8, b = color >> 16;
        // if(r > g && r > b) {
        //     color = r;
        // } else if(g > b) {
        //     color = ((uint16_t)g) << 8;
        // } else {
        //     color = ((uint32_t)b) << 16;
        // }
        info("Created Agent with color %06X and direction %.2f\n", color, direction);
    }

    void update(float delta_time) {
        coords next_pos;

        float weightForward = sense(0);
        float weightLeft = sense(sensor_angle_rad);
        float weightRight = sense(-sensor_angle_rad);

        float randomSteerStrength = ((float)hash() / (float)UINT32_MAX);

        // Continue in same direction
        if (weightForward > weightLeft && weightForward > weightRight) {
            direction += 0;
        }
        else if (weightForward < weightLeft && weightForward < weightRight) {
            direction += (randomSteerStrength - 0.5) * 2 * turnSpeed * delta_time;
        }
        // Turn right
        else if (weightRight > weightLeft) {
            direction -= randomSteerStrength * turnSpeed * delta_time;
        }
        // Turn left
        else if (weightLeft > weightRight) {
            direction += randomSteerStrength * turnSpeed * delta_time;
        }

        next_pos.x = position.x + speed * delta_time * cos(direction);
        next_pos.y = position.y + speed * delta_time * sin(direction);
        if(next_pos.x > 63 || next_pos.y > 63 || next_pos.x < 0 || next_pos.y < 0) {
            direction = ((float)hash() / (float)UINT32_MAX) * M_TWOPI;
            next_pos.x = std::min(63.0f, std::max(0.0f, next_pos.x));
            next_pos.y = std::min(63.0f, std::max(0.0f, next_pos.y));
        }

        trails->add((uint8_t)roundf(position.x), (uint8_t)roundf(position.y), color);
        position = next_pos;
    }

    float sense(float angle_offset) {
        float sense_angle = direction + angle_offset;
        uint8_t pos_x = (uint8_t)roundf(position.x + cos(sense_angle) * sense_offset);
        uint8_t pos_y = (uint8_t)roundf(position.y + sin(sense_angle) * sense_offset);
        return (float)(trails->get(pos_x, pos_y) & 0x00FFFFFF) / (float)0xFFFFFFu;
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
    for(int i = 0; i < 275; i++) {
        agents.push_back(new Agent(center, trails));
    }
    double smooth_frametime = 0.016;
    float frame_time = 0.016;
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
                (*it)->update(frame_time);
            }
            create_frame = false;
        } else if(time_reached(frame_target) && matrix->flipped()) {
            //info1("Flipping buffer...\n");
            matrix->flip_buffer();
            absolute_time_t curr = get_absolute_time();
            frame_time = (float)(((double)absolute_time_diff_us(start_time, curr)) / 1000000.0);
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