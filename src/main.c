#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"

#include "imu.h"
#include "oled.h"
#include "ws2812.pio.h"

// I2C is shared by the OLED and MAX17048 fuel gauge
#define I2C_PORT    i2c1
#define I2C_SDA_PIN 10
#define I2C_SCL_PIN 11

// WS2812 configuration
#define LED_PIN         8
#define NUM_LEDS        4
#define IS_RGBW         false
#define LED_FREQ_HZ     800000
#define STEPS_PER_LED   250

// Update cadences (ms)
#define IMU_SAMPLE_MS       20   // ~50 Hz for step detection
#define DISPLAY_REFRESH_MS  250
#define BATTERY_SAMPLE_MS   1000

// Fuel-gauge API (implemented in max17048.c)
float read_voltage(void);
float read_soc(void);
int quickstart(void);

// WS2812 helpers (implemented in ws2812.c)
uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b);
void put_pixel(uint32_t pixel_grb);

static void i2c_bus_init(void) {
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

static uint8_t clamp_percent(float soc) {
    if (soc < 0.0f) return 0;
    if (soc > 100.0f) return 100;
    return (uint8_t)(soc + 0.5f); // round to nearest
}

// Map 0% -> red, 50% -> yellow, 100% -> green
static void battery_color(uint8_t percent, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (percent <= 50) {
        *r = 255;
        *g = (uint8_t)((percent * 255) / 50);
    } else {
        *r = (uint8_t)(255 - (((percent - 50) * 255) / 50));
        *g = 255;
    }
    *b = 0;
}

static void update_led_bar(uint32_t steps, uint8_t battery_percent) {
    uint8_t r = 0, g = 0, b = 0;
    battery_color(battery_percent, &r, &g, &b);

    uint32_t leds_on_raw = steps / STEPS_PER_LED;
    uint8_t leds_on = (leds_on_raw > NUM_LEDS) ? NUM_LEDS : (uint8_t)leds_on_raw;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        if (i < leds_on) {
            put_pixel(rgb_to_grb(r, g, b));
        } else {
            put_pixel(rgb_to_grb(0, 0, 0));
        }
    }
}

static void render_oled(uint32_t steps, uint8_t battery_percent) {
    oled_home();
    oled_print(4, 2, "STEPS");
    oled_show_battery(battery_percent);
    oled_show_steps(steps);
    oled_display();
}

int main(void) {
    stdio_init_all();
    sleep_ms(200); // give USB time to enumerate

    i2c_bus_init();

    if (quickstart() != 0) {
        printf("MAX17048 quickstart failed\n");
    }

    bool oled_ok = oled_init();
    if (!oled_ok) {
        printf("OLED init failed!\n");
    }

    bool imu_ok = imu_init();
    if (!imu_ok) {
        printf("IMU init failed!\n");
    }

    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, LED_PIN, LED_FREQ_HZ, IS_RGBW);
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        put_pixel(rgb_to_grb(0, 0, 0)); // clear strip
    }

    float soc = read_soc();
    if (soc < 0.0f) soc = 0.0f;
    uint8_t battery_percent = clamp_percent(soc);
    uint32_t last_imu_ms = 0;
    uint32_t last_display_ms = 0;
    uint32_t last_battery_ms = 0;

    while (true) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        if ((now_ms - last_imu_ms) >= IMU_SAMPLE_MS) {
            last_imu_ms = now_ms;
            if (imu_ok) {
                imu_update(now_ms);
            }
        }

        if ((now_ms - last_battery_ms) >= BATTERY_SAMPLE_MS) {
            last_battery_ms = now_ms;
            float soc_read = read_soc();
            if (soc_read >= 0.0f) {
                soc = soc_read;
                battery_percent = clamp_percent(soc);
            }
        }

        uint32_t steps = imu_ok ? imu_get_total_steps() : 0;
        update_led_bar(steps, battery_percent);

        if ((now_ms - last_display_ms) >= DISPLAY_REFRESH_MS) {
            last_display_ms = now_ms;
            render_oled(steps, battery_percent);
            printf("steps=%lu soc=%.1f%%\n", steps, soc);
        }

        sleep_ms(5);
    }
}
