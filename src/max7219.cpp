#include "max7219.h"

void max7219_write_reg(uint8_t addr, uint8_t data) {
    trace("max7219_write_reg %x %02x\n", addr, data);
    //write_serial(((uint16_t)addr << 8) | data);
    uint8_t buf[2] = {addr, data};
    cs_select();
    spi_write_blocking(spi_default, buf, 2);
    cs_deselect();
    sleep_us(MAX7219_DESELECT_DELAY);
}

void max7219_init() {
    spi_init(spi_default, 10000000);
    gpio_set_function(MAX7219_CLK_GPIO, GPIO_FUNC_SPI);
    gpio_set_function(MAX7219_TX_GPIO, GPIO_FUNC_SPI);

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(MAX7219_LOAD_GPIO);
    gpio_set_dir(MAX7219_LOAD_GPIO, GPIO_OUT);
    sleep_ms(10);

    max7219_write_reg(MAX7219_REG_SHUTDOWN, 0x01);
    max7219_write_reg(MAX7219_REG_INTENSITY, INTENSITY);
    max7219_write_reg(MAX7219_REG_SCANLIMIT, SCANLIMIT);
    max7219_write_reg(MAX7219_REG_DECMODE, 0x00);
    max7219_write_reg(MAX7219_REG_DIGIT0, 0x00);
    max7219_write_reg(MAX7219_REG_DIGIT1, 0x00);
    max7219_write_reg(MAX7219_REG_DIGIT2, 0x00);
    max7219_write_reg(MAX7219_REG_DIGIT3, 0x00);
}

void max7219_ensure_init() {
    max7219_write_reg(MAX7219_REG_SHUTDOWN, 0x01);
    max7219_write_reg(MAX7219_REG_INTENSITY, INTENSITY);
    max7219_write_reg(MAX7219_REG_SCANLIMIT, SCANLIMIT);
    max7219_write_reg(MAX7219_REG_DECMODE, DECMODE);
}

void max7219_write(float num) {
    int value = (int)(num * 10);
    for(int i = 0; i < 4; i++) {
        uint8_t digit = ((int)(value / pow(10, i))) % 10;
        if(i == 1) {
            digit |= 0x80;
        } 
        if(num < 100.0f && i == 3 || num < 10.0f && i >= 2) {
            digit = 0x0F;
        }
        max7219_write_reg(MAX7219_REG_DIGIT0 + i, digit);
    }
}

void max7219_set_brightness(float percent) {
    if(percent < 0) {
        percent = 0;
    }
    if(percent > 1) {
        percent = 1;
    }

    max7219_write_reg(MAX7219_REG_INTENSITY, 0x0F * percent);
}