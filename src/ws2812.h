#ifndef WS2812_HELPERS_H
#define WS2812_HELPERS_H

#include <stdint.h>
#include <stdbool.h>

// Basic WS2812 helpers implemented in ws2812.c
uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b);
void put_pixel(uint32_t pixel_grb);

// High-level LED bar helper used by main application code.
// Renders a step-progress bar whose color encodes battery percentage.
void update_led_bar(uint32_t steps, uint8_t battery_percent);

#endif // WS2812_HELPERS_H


