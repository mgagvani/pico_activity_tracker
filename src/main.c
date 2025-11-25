#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"    // Make sure you include the PIO program from the Pico SDK example

// === MODIFY THESE TO MATCH YOUR PCB ===
#define LED_PIN     8       // GPIO connected to DIN of first WS2812B
#define NUM_LEDS    4       // You have 4 LEDs in your schematic
#define IS_RGBW     false   // WS2812B = RGB only

// Convert individual R,G,B into a single 24-bit GRB format (WS2812 uses GRB!)
uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    return (g << 16) | (r << 8) | b;
}

// Send one pixel value to the LED strip
void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Fill entire strip with one color
void fill_strip(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t grb = rgb_to_grb(r, g, b);
    for (int i = 0; i < NUM_LEDS; i++) {
        put_pixel(grb);
    }
}

int main() {
    stdio_init_all();

    // Load WS2812 program into a PIO
    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, LED_PIN, 800000, IS_RGBW);

    // Turn everything RED
    while (true) {
        fill_strip(0, 225, 0);   // G, R, B = RED
        sleep_ms(100);
    }
}