#include "ws2812.h"
#include "ws2812.pio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

#define NUM_LEDS       4
#define STEPS_PER_LED  25

uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    return (g << 16) | (r << 8) | b;
}

void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Smooth red -> yellow -> green gradient across 0-100%
static void battery_color(uint8_t percent, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t p = (percent > 100) ? 100 : percent;
    // Swapped channels to test physical wiring/order: green now tracks low charge,
    // red tracks high charge. Expect green at 100%, red at 0% if channels were reversed.
    *g = (uint8_t)((255 * (100 - p)) / 100); // 255 at 0%, 0 at 100%
    *r = (uint8_t)((255 * p) / 100);         // 0 at 0%, 255 at 100%
    *b = 0;
}

void update_led_bar(uint32_t steps, uint8_t battery_percent) {
    uint8_t r = 0, g = 0, b = 0;
    battery_color(battery_percent, &r, &g, &b);

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        int32_t steps_into_segment = (int32_t)steps - (int32_t)(i * STEPS_PER_LED);
        if (steps_into_segment <= 0) {
            put_pixel(rgb_to_grb(0, 0, 0));
            continue;
        }
        if (steps_into_segment > STEPS_PER_LED) steps_into_segment = STEPS_PER_LED;
        uint8_t scale = (uint8_t)((steps_into_segment * 255) / STEPS_PER_LED); // 0-255 brightness
        uint8_t sr = (uint8_t)((r * scale) / 255);
        uint8_t sg = (uint8_t)((g * scale) / 255);
        uint8_t sb = (uint8_t)((b * scale) / 255);
        put_pixel(rgb_to_grb(sr, sg, sb));
    }
}
