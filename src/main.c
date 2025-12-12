#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"

#include "imu.h"
#include "oled.h"
#include "steps_to_calories.h"
#include "ws2812.pio.h"
#include "ws2812.h"

// I2C is shared by the OLED and MAX17048 fuel gauge
#define I2C_PORT    i2c1
#define I2C_SDA_PIN 10
#define I2C_SCL_PIN 11

// WS2812 configuration
#define LED_PIN         8
#define NUM_LEDS        4
#define IS_RGBW         false
#define LED_FREQ_HZ     800000

// Button GPIOs on Proton board
#define BUTTON_MODE_PIN  26  // Toggle display between steps and calories
#define BUTTON_START_PIN 21  // Start/pause/continue workout

// User configuration for calorie estimates
#define USER_WEIGHT_LBS       160
#define USER_HEIGHT_CATEGORY  HEIGHT_MEDIUM

// Update cadences (ms)
#define IMU_SAMPLE_MS       20   // ~50 Hz for step detection
#define DISPLAY_REFRESH_MS  250
#define BATTERY_SAMPLE_MS   1000
#define DIAG_INTERVAL_MS    500  // Serial diagnostics cadence

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

static void buttons_init(void) {
    gpio_init(BUTTON_MODE_PIN);
    gpio_set_dir(BUTTON_MODE_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_MODE_PIN);   // Active-low button with pull-up

    gpio_init(BUTTON_START_PIN);
    gpio_set_dir(BUTTON_START_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_START_PIN);  // Active-low button with pull-up
}

static void render_oled(uint32_t steps, uint32_t calories, uint8_t battery_percent,
                        bool show_calories, bool paused) {
    oled_home();

    if (show_calories) {
        oled_print(4, 2, "CAL");
        oled_show_battery(battery_percent);
        oled_show_calories(calories);
    } else {
        oled_print(4, 2, "STEPS");
        oled_show_battery(battery_percent);
        oled_show_steps(steps);
    }

    if (paused) {
        // Show "PAUSED" near the bottom of the 32px display
        oled_print(32, 24, "PAUSED");
    }

    oled_display();
}

int main(void) {
    stdio_init_all();
    sleep_ms(200); // give USB time to enumerate

    i2c_bus_init();
    buttons_init();

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
    uint32_t last_diag_ms = 0;
    uint32_t prev_steps = 0;
    bool show_calories = false;
    bool last_mode_level = true;       // pull-up, so idle = high
    uint32_t last_mode_toggle_ms = 0;  // debounce timer
    bool workout_running = false;
    bool last_start_level = true;      // pull-up, so idle = high
    uint32_t last_start_toggle_ms = 0; // debounce timer
    uint32_t workout_steps = 0;
    uint32_t workout_offset = 0;       // IMU total steps at workout (re)start

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

        uint32_t total_steps = imu_ok ? imu_get_total_steps() : 0;

        // Handle mode button (GPIO 26): toggle between steps and calories
        bool mode_level = gpio_get(BUTTON_MODE_PIN);  // 1 = released, 0 = pressed (active-low)
        if (!mode_level && last_mode_level &&
            (now_ms - last_mode_toggle_ms) > 200) {   // simple debounce
            show_calories = !show_calories;
            last_mode_toggle_ms = now_ms;
            printf("Mode button pressed -> display %s\n",
                   show_calories ? "CALORIES" : "STEPS");
        }
        last_mode_level = mode_level;

        // Handle start/pause button (GPIO 21): control workout state
        bool start_level = gpio_get(BUTTON_START_PIN); // 1 = released, 0 = pressed (active-low)
        if (!start_level && last_start_level &&
            (now_ms - last_start_toggle_ms) > 200) {   // simple debounce
            workout_running = !workout_running;
            last_start_toggle_ms = now_ms;

            if (workout_running) {
                // Starting or resuming: align offset so steps don't jump
                workout_offset = total_steps - workout_steps;
                printf("Workout %s\n", (workout_steps == 0) ? "START" : "RESUME");
            } else {
                printf("Workout PAUSE\n");
            }
        }
        last_start_level = start_level;

        // Update workout step count only while running
        if (workout_running) {
            if (total_steps >= workout_offset) {
                workout_steps = total_steps - workout_offset;
            } else {
                workout_steps = 0;
                workout_offset = total_steps;
            }
        }

        uint32_t calories = steps_to_calories(workout_steps, USER_WEIGHT_LBS, USER_HEIGHT_CATEGORY);

        if (workout_steps != prev_steps) {
            printf("STEP %lu @ %lums\n", workout_steps, now_ms);
            prev_steps = workout_steps;
        }

        if ((now_ms - last_diag_ms) >= DIAG_INTERVAL_MS) {
            last_diag_ms = now_ms;
            int16_t ax = 0, ay = 0, az = 0;
            float fax = 0.0f, fay = 0.0f, faz = 0.0f;
            float mag = 0.0f;

            if (imu_ok) {
                imu_get_accel_raw(&ax, &ay, &az);
                imu_get_accel_filtered(&fax, &fay, &faz);
                mag = sqrtf(fax * fax + fay * fay + faz * faz);
                printf("diag t=%lums raw=(%6d,%6d,%6d) g=(%.3f,%.3f,%.3f) |g|=%.3f steps=%lu batt=%u%%\n",
                       now_ms, ax, ay, az, fax, fay, faz, mag, workout_steps, battery_percent);
            } else {
                printf("diag t=%lums IMU not initialized, steps=%lu batt=%u%%\n",
                       now_ms, workout_steps, battery_percent);
            }
        }

        update_led_bar(workout_steps, battery_percent);

        if ((now_ms - last_display_ms) >= DISPLAY_REFRESH_MS) {
            last_display_ms = now_ms;
            bool paused = (!workout_running && (workout_steps > 0));
            render_oled(workout_steps, calories, battery_percent, show_calories, paused);
            printf("steps=%lu cal=%lu soc=%.1f%% %s\n",
                   workout_steps, calories, soc, paused ? "[PAUSED]" : "");
        }

        sleep_ms(5);
    }
}
