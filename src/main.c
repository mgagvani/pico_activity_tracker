#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

#define LED_PIN     8
#define NUM_LEDS    4
#define IS_RGBW     false

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

int main() {
    stdio_init_all();

    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, LED_PIN, 800000, IS_RGBW);

    // Fade color storage (NOT displayed directly)
    uint8_t r[NUM_LEDS];
    uint8_t g[NUM_LEDS];
    uint8_t b[NUM_LEDS];

    // Start GREEN
    for (int i = 0; i < NUM_LEDS; i++) {
        r[i] = 225;
        g[i] = 0;
        b[i] = 0;
    }

    int blink_index = 0;
    const int blink_interval_ms = 100;

    const int fade_step_ms   = 2;
    const int total_fade_ms  = 200;
    const int fade_steps     = total_fade_ms / fade_step_ms;

    int fade_counter = 0;

    while (true) {

        // --- Update fade values only ---
        if (fade_counter < fade_steps) {
            for (int i = 0; i < NUM_LEDS; i++) {
                // fade from green → red
                r[i] = 255 - (fade_counter * (255.0 / fade_steps));
                g[i] = (fade_counter * (255.0 / fade_steps));
                b[i] = 0;
            }
            fade_counter++;
        }

        // --- BLINK SEQUENCE for the ACTIVE LED ---
        // OFF
        show_one_led(blink_index, r, g, b, true);
        sleep_ms(blink_interval_ms / 2);

        // ON (with fade color)
        show_one_led(blink_index, r, g, b, false);
        sleep_ms(blink_interval_ms / 2);

        // Next LED
        blink_index = (blink_index + 1) % NUM_LEDS;
    }
}
