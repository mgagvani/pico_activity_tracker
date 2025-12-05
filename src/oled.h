#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include <stdbool.h>

// ==============================
//  SSD1306 OLED Display Driver
// ==============================
//
// Common 128x64 or 128x32 pixel OLED displays
// using the SSD1306 controller over I2C.
//

// Display dimensions
#define OLED_WIDTH   128
#define OLED_HEIGHT  32

// Initialize the OLED display
// Returns true on success, false on failure
bool oled_init(void);

// Clear the display buffer and update the screen
void oled_clear(void);

// Update the display with the current buffer contents
void oled_display(void);

// Set a single pixel in the buffer
// x: 0 to OLED_WIDTH-1
// y: 0 to OLED_HEIGHT-1
// color: 1 = on, 0 = off
void oled_set_pixel(int16_t x, int16_t y, uint8_t color);

// Set the cursor position for text
void oled_set_cursor(uint8_t x, uint8_t y);

// Write a single character at the current cursor position
void oled_write_char(char c);

// Write a string at the current cursor position
void oled_write_string(const char *str);

// Write a string at a specific position (convenience function)
void oled_print(uint8_t x, uint8_t y, const char *str);

// Draw a filled rectangle
void oled_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);

// Draw a horizontal line
void oled_draw_hline(int16_t x, int16_t y, int16_t w, uint8_t color);

// Draw a vertical line
void oled_draw_vline(int16_t x, int16_t y, int16_t h, uint8_t color);

// Invert the display colors (0 = normal, 1 = inverted)
void oled_invert(bool invert);

// Set display contrast (0-255)
void oled_set_contrast(uint8_t contrast);

// Draw battery icon with 5 states (0%, 25%, 50%, 75%, 100%)
// x, y: top-left corner position
// percent: battery level 0-100
void oled_draw_battery(int16_t x, int16_t y, uint8_t percent);

// Write text at 2x scale for large numbers
void oled_print_2x(uint8_t x, uint8_t y, const char *str);

// Draw a rectangle outline
void oled_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);

// ==============================
//  Easy-to-use convenience functions
// ==============================

// Print at current cursor position (no coordinates needed)
// Cursor auto-advances after each character
void oled_puts(const char *str);

// Print and move cursor to next line
void oled_println(const char *str);

// Printf-style formatted print at current cursor
// Example: oled_printf("Steps: %d", steps);
void oled_printf(const char *fmt, ...);

// Show battery icon in top-right corner (default position)
// Just pass the percentage, no coordinates needed
void oled_show_battery(uint8_t percent);

// Show step count centered on display (large 2x text)
// Automatically centers the number horizontally
void oled_show_steps(uint32_t steps);

// Clear screen and reset cursor to top-left
void oled_home(void);

// Move cursor to specific line (0-3 for 32px display)
// Each line is 8 pixels tall
void oled_goto_line(uint8_t line);

#endif // OLED_H

