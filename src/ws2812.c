#include "ws2812.pio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

#define NUM_LEDS 4


uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    return (g << 16) | (r << 8) | b;
}

void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

void show_one_led(int active, uint8_t r[], uint8_t g[], uint8_t b[], bool off) {
    for (int i = 0; i < NUM_LEDS; i++) {
        if (i == active) {
            if (off)
                put_pixel(rgb_to_grb(0,0,0));   // blink OFF
            else
                put_pixel(rgb_to_grb(r[i], g[i], b[i])); // blink ON using fade color
        } else {
            put_pixel(rgb_to_grb(0,0,0)); // all others OFF
        }
    }
}