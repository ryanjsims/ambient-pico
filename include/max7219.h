#pragma once

#include <math.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>

#include "logger.h"

#define MAX7219_DESELECT_DELAY 10

#define MAX7219_LOAD_GPIO 1
#define MAX7219_CLK_GPIO  2
#define MAX7219_TX_GPIO   3

#define MAX7219_REG_NOOP      0x0
#define MAX7219_REG_DIGIT0    0x1
#define MAX7219_REG_DIGIT1    0x2
#define MAX7219_REG_DIGIT2    0x3
#define MAX7219_REG_DIGIT3    0x4
#define MAX7219_REG_DIGIT4    0x5
#define MAX7219_REG_DIGIT5    0x6
#define MAX7219_REG_DIGIT6    0x7
#define MAX7219_REG_DIGIT7    0x8
#define MAX7219_REG_DECMODE   0x9
#define MAX7219_REG_INTENSITY 0xA
#define MAX7219_REG_SCANLIMIT 0xB
#define MAX7219_REG_SHUTDOWN  0xC


static inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(MAX7219_LOAD_GPIO, 0);  // Active low
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(MAX7219_LOAD_GPIO, 1);
    asm volatile("nop \n nop \n nop");
}

void max7219_write_reg(uint8_t addr, uint8_t data);

// Initializes SPI and writes startup data to max7219
void max7219_init();

// Writes 4 digits of a float (hundreds, tens, ones, and tenths) to lower 4 digits of max7219
void max7219_write(float num);