/*
 * SSD1306 OLED Display Driver
 * 
 * Drives 128x64 I2C OLED displays using the SSD1306 controller.
 * Uses a local framebuffer that gets pushed to the display on oled_display().
 */

#include "oled.h"
#include "font5x7.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define OLED_I2C_PORT   i2c1
#define OLED_I2C_SDA    10
#define OLED_I2C_SCL    11
#define SSD1306_ADDR    0x3C    // Common address; some displays use 0x3D

#define SSD1306_CMD     0x00    // I2C control byte: next byte is a command
#define SSD1306_DATA    0x40    // I2C control byte: following bytes are GDDRAM data

// SSD1306 command definitions
#define SSD1306_DISPLAY_OFF         0xAE
#define SSD1306_DISPLAY_ON          0xAF
#define SSD1306_SET_CONTRAST        0x81
#define SSD1306_NORMAL_DISPLAY      0xA6
#define SSD1306_INVERT_DISPLAY      0xA7
#define SSD1306_SET_MUX_RATIO       0xA8
#define SSD1306_SET_DISP_OFFSET     0xD3
#define SSD1306_SET_DISP_CLK_DIV    0xD5
#define SSD1306_SET_PRECHARGE       0xD9
#define SSD1306_SET_COM_PINS        0xDA
#define SSD1306_SET_VCOM_DESEL      0xDB
#define SSD1306_SET_START_LINE      0x40
#define SSD1306_CHARGE_PUMP         0x8D
#define SSD1306_MEM_ADDR_MODE       0x20
#define SSD1306_SET_COL_ADDR        0x21
#define SSD1306_SET_PAGE_ADDR       0x22
#define SSD1306_SEG_REMAP           0xA0
#define SSD1306_COM_SCAN_DEC        0xC8
#define SSD1306_DISPLAY_ALL_ON_RES  0xA4

#define BUFFER_SIZE ((OLED_WIDTH * OLED_HEIGHT) / 8)

static uint8_t s_buffer[BUFFER_SIZE];
static uint8_t s_cursor_x = 0;
static uint8_t s_cursor_y = 0;

static void oled_send_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {SSD1306_CMD, cmd};
    i2c_write_blocking(OLED_I2C_PORT, SSD1306_ADDR, buf, 2, false);
}

static void oled_send_data(const uint8_t *data, size_t len)
{
    uint8_t buf[129];
    buf[0] = SSD1306_DATA;
    
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = (len - offset > 128) ? 128 : (len - offset);
        memcpy(buf + 1, data + offset, chunk);
        i2c_write_blocking(OLED_I2C_PORT, SSD1306_ADDR, buf, chunk + 1, false);
        offset += chunk;
    }
}

bool oled_init(void)
{
    sleep_ms(100);  // Allow display power-up
    
    // Initialization sequence configures: display off, clock, multiplexing,
    // memory addressing mode, segment remapping, COM scan direction, 
    // charge pump (required for internal DC-DC), contrast, and display on.
    static const uint8_t init_cmds[] = {
        SSD1306_DISPLAY_OFF,
        SSD1306_SET_DISP_CLK_DIV, 0x80,
        SSD1306_SET_MUX_RATIO, OLED_HEIGHT - 1,
        SSD1306_SET_DISP_OFFSET, 0x00,
        SSD1306_SET_START_LINE | 0x00,
        SSD1306_CHARGE_PUMP, 0x14,              // Enable internal charge pump
        SSD1306_MEM_ADDR_MODE, 0x00,            // Horizontal addressing mode
        SSD1306_SEG_REMAP | 0x01,               // Flip horizontally
        SSD1306_COM_SCAN_DEC,                   // Flip vertically
        SSD1306_SET_COM_PINS, 0x02,             // Sequential COM pin config for 128x32
        SSD1306_SET_CONTRAST, 0xCF,
        SSD1306_SET_PRECHARGE, 0xF1,
        SSD1306_SET_VCOM_DESEL, 0x40,
        SSD1306_DISPLAY_ALL_ON_RES,
        SSD1306_NORMAL_DISPLAY,
        SSD1306_DISPLAY_ON,
    };
    
    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        oled_send_cmd(init_cmds[i]);
    }
    
    oled_clear();
    return true;
}

void oled_clear(void)
{
    memset(s_buffer, 0, BUFFER_SIZE);
    oled_display();
}

void oled_display(void)
{
    // Set draw window to entire display, then push framebuffer
    oled_send_cmd(SSD1306_SET_COL_ADDR);
    oled_send_cmd(0);
    oled_send_cmd(OLED_WIDTH - 1);
    oled_send_cmd(SSD1306_SET_PAGE_ADDR);
    oled_send_cmd(0);
    oled_send_cmd((OLED_HEIGHT / 8) - 1);
    oled_send_data(s_buffer, BUFFER_SIZE);
}

void oled_set_pixel(int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    
    // Buffer is organized in 8 horizontal pages; each byte is a vertical 8-pixel strip
    uint16_t idx = x + (y / 8) * OLED_WIDTH;
    uint8_t bit = 1 << (y & 7);
    
    if (color) s_buffer[idx] |= bit;
    else       s_buffer[idx] &= ~bit;
}

void oled_set_cursor(uint8_t x, uint8_t y)
{
    s_cursor_x = x;
    s_cursor_y = y;
}

void oled_write_char(char c)
{
    if (c == '\n') {
        s_cursor_x = 0;
        s_cursor_y += FONT_HEIGHT;
        return;
    }
    
    if (c < 32 || c > 126) c = ' ';
    uint8_t idx = c - FONT_FIRST_CHAR;
    
    // Draw each column of the character glyph
    for (uint8_t col = 0; col < FONT_WIDTH; col++) {
        uint8_t line = font5x7[idx][col];
        for (uint8_t row = 0; row < FONT_HEIGHT; row++) {
            if (line & (1 << row)) {
                oled_set_pixel(s_cursor_x + col, s_cursor_y + row, 1);
            }
        }
    }
    
    s_cursor_x += FONT_WIDTH + 1;  // Advance cursor with 1px spacing
    if (s_cursor_x + FONT_WIDTH > OLED_WIDTH) {
        s_cursor_x = 0;
        s_cursor_y += FONT_HEIGHT;
    }
}

void oled_write_string(const char *str)
{
    while (*str) oled_write_char(*str++);
}

void oled_print(uint8_t x, uint8_t y, const char *str)
{
    oled_set_cursor(x, y);
    oled_write_string(str);
}

void oled_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    for (int16_t i = x; i < x + w; i++) {
        for (int16_t j = y; j < y + h; j++) {
            oled_set_pixel(i, j, color);
        }
    }
}

void oled_draw_hline(int16_t x, int16_t y, int16_t w, uint8_t color)
{
    for (int16_t i = x; i < x + w; i++) oled_set_pixel(i, y, color);
}

void oled_draw_vline(int16_t x, int16_t y, int16_t h, uint8_t color)
{
    for (int16_t j = y; j < y + h; j++) oled_set_pixel(x, j, color);
}

void oled_invert(bool invert)
{
    oled_send_cmd(invert ? SSD1306_INVERT_DISPLAY : SSD1306_NORMAL_DISPLAY);
}

void oled_set_contrast(uint8_t contrast)
{
    oled_send_cmd(SSD1306_SET_CONTRAST);
    oled_send_cmd(contrast);
}

void oled_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    oled_draw_hline(x, y, w, color);              // Top
    oled_draw_hline(x, y + h - 1, w, color);      // Bottom
    oled_draw_vline(x, y, h, color);              // Left
    oled_draw_vline(x + w - 1, y, h, color);      // Right
}

// Battery icon: 24x12 pixels with terminal nub on right
// 5 states: empty, 25%, 50%, 75%, 100% (4 fill segments)
void oled_draw_battery(int16_t x, int16_t y, uint8_t percent)
{
    // Battery body outline (20x12)
    oled_draw_rect(x, y, 20, 12, 1);
    
    // Battery terminal nub on right (4x6, centered vertically)
    oled_fill_rect(x + 20, y + 3, 3, 6, 1);
    
    // Determine fill level (4 segments, each 4px wide with 1px gaps)
    uint8_t bars = 0;
    if (percent >= 87) bars = 4;
    else if (percent >= 62) bars = 3;
    else if (percent >= 37) bars = 2;
    else if (percent >= 12) bars = 1;
    
    // Draw fill segments (each 4x8, starting 2px from left edge)
    for (uint8_t i = 0; i < bars; i++) {
        int16_t bar_x = x + 2 + (i * 4) + i;  // 4px wide + 1px gap
        oled_fill_rect(bar_x, y + 2, 4, 8, 1);
    }
}

// Write a single character at 2x scale
static void oled_write_char_2x(uint8_t x, uint8_t y, char c)
{
    if (c < 32 || c > 126) c = ' ';
    uint8_t idx = c - FONT_FIRST_CHAR;
    
    // Scale each pixel 2x in both dimensions
    for (uint8_t col = 0; col < FONT_WIDTH; col++) {
        uint8_t line = font5x7[idx][col];
        for (uint8_t row = 0; row < FONT_HEIGHT; row++) {
            if (line & (1 << row)) {
                // Draw 2x2 block for each pixel
                oled_set_pixel(x + col * 2,     y + row * 2,     1);
                oled_set_pixel(x + col * 2 + 1, y + row * 2,     1);
                oled_set_pixel(x + col * 2,     y + row * 2 + 1, 1);
                oled_set_pixel(x + col * 2 + 1, y + row * 2 + 1, 1);
            }
        }
    }
}

void oled_print_2x(uint8_t x, uint8_t y, const char *str)
{
    uint8_t cursor_x = x;
    while (*str) {
        oled_write_char_2x(cursor_x, y, *str++);
        cursor_x += (FONT_WIDTH * 2) + 2;  // 2x width + 2px spacing
    }
}

// ==============================
//  Easy-to-use convenience functions
// ==============================

void oled_puts(const char *str)
{
    oled_write_string(str);
}

void oled_println(const char *str)
{
    oled_write_string(str);
    s_cursor_x = 0;
    s_cursor_y += FONT_HEIGHT;
}

void oled_printf(const char *fmt, ...)
{
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    oled_write_string(buf);
}

void oled_show_battery(uint8_t percent)
{
    // Draw battery in top-right corner (default position)
    oled_draw_battery(100, 2, percent);
}

void oled_show_steps(uint32_t steps)
{
    // Format step count
    char buf[12];
    snprintf(buf, sizeof(buf), "%lu", steps);
    
    // Calculate width to center horizontally (2x scale: 12px per char)
    uint8_t len = strlen(buf);
    uint8_t text_width = len * 12 - 2;
    uint8_t x = (OLED_WIDTH - text_width) / 2;
    
    // Draw centered, vertically positioned for 32px display
    oled_print_2x(x, 14, buf);
}

void oled_show_calories(uint32_t calories)
{
    // Format calorie count
    char buf[12];
    snprintf(buf, sizeof(buf), "%lu", calories);

    // Calculate width to center horizontally (2x scale: 12px per char)
    uint8_t len = strlen(buf);
    uint8_t text_width = len * 12 - 2;
    uint8_t x = (OLED_WIDTH - text_width) / 2;

    // Draw centered, vertically positioned for 32px display
    oled_print_2x(x, 14, buf);
}

void oled_home(void)
{
    memset(s_buffer, 0, BUFFER_SIZE);
    s_cursor_x = 0;
    s_cursor_y = 0;
}

void oled_goto_line(uint8_t line)
{
    s_cursor_x = 0;
    s_cursor_y = line * FONT_HEIGHT;
    if (s_cursor_y >= OLED_HEIGHT) {
        s_cursor_y = 0;
    }
}

