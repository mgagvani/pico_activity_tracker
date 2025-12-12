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
#include "font5x7.h"
#include "steps_to_calories.h"
#include "ws2812.pio.h"

// I2C is shared by the OLED and MAX17048 fuel gauge
#define I2C_PORT    i2c1
#define I2C_SDA_PIN 14
#define I2C_SCL_PIN 15

// WS2812 configuration
#define LED_PIN         8
#define NUM_LEDS        4
#define IS_RGBW         false
#define LED_FREQ_HZ     800000
#define STEPS_PER_LED   25
#define BREATH_PERIOD_MS 3000

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

// Smooth red -> yellow -> green gradient across 0-100%
static void battery_color(uint8_t percent, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t p = (percent > 100) ? 100 : percent;
    // Swapped channels to test physical wiring/order: green now tracks low charge,
    // red tracks high charge. Expect green at 100%, red at 0% if channels were reversed.
    *g = (uint8_t)((255 * (100 - p)) / 100); // 255 at 0%, 0 at 100%
    *r = (uint8_t)((255 * p) / 100);         // 0 at 0%, 255 at 100%
    *b = 0;
}

static void update_led_bar(uint32_t steps, uint8_t battery_percent, uint32_t now_ms) {
    uint8_t r = 0, g = 0, b = 0;
    battery_color(battery_percent, &r, &g, &b);

    // Breathing blue for unlit/partial LEDs
    float phase = (float)(now_ms % BREATH_PERIOD_MS) / (float)BREATH_PERIOD_MS; // 0-1
    float breath = (sinf(phase * 2.0f * (float)M_PI) + 1.0f) * 0.5f;             // 0-1
    uint8_t breath_level = (uint8_t)(breath * 80.0f); // cap breathing brightness

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        int32_t steps_into_segment = (int32_t)steps - (int32_t)(i * STEPS_PER_LED);
        if (steps_into_segment <= 0) {
            put_pixel(rgb_to_grb(0, 0, breath_level));
            continue;
        }
        if (steps_into_segment > STEPS_PER_LED) steps_into_segment = STEPS_PER_LED;
        uint8_t scale = (uint8_t)((steps_into_segment * 255) / STEPS_PER_LED); // 0-255 brightness
        uint8_t sr = (uint8_t)((r * scale) / 255);
        uint8_t sg = (uint8_t)((g * scale) / 255);
        uint8_t sb = (uint8_t)((b * scale) / 255);
        if (scale < 255) {
            // Add breathing blue when not fully lit
            uint16_t boosted_b = sb + breath_level;
            sb = (boosted_b > 255) ? 255 : (uint8_t)boosted_b;
        }
        put_pixel(rgb_to_grb(sr, sg, sb));
    }
}

typedef struct {
    uint16_t frame_idx;
    uint16_t frames_per_travel;
    uint8_t last_pos;
} led_bounce_state_t;

static void startup_led_bounce_step(void *ctx, int16_t x_unused) {
    (void)x_unused;
    led_bounce_state_t *state = (led_bounce_state_t *)ctx;
    if (state->frames_per_travel == 0) state->frames_per_travel = 1;

    if (NUM_LEDS == 0) return;
    uint8_t end = NUM_LEDS - 1;
    uint16_t travel = state->frame_idx / state->frames_per_travel;
    uint16_t within = state->frame_idx % state->frames_per_travel;

    bool forward = (travel & 0x1) == 0;

    float t = 0.0f;
    if (state->frames_per_travel > 1) {
        t = (float)within / (float)(state->frames_per_travel - 1);
    }

    // Position along strip with a smooth cross-fade between LEDs
    float posf = t * (float)end;
    if (!forward) posf = (float)end - posf;

    uint8_t base = (uint8_t)posf;
    if (base > end) base = end;
    float frac = posf - (float)base; // 0..1 between base and next

    uint8_t head_level = (uint8_t)(80.0f + frac * 20.0f + 0.5f);    // 80 -> 100
    uint8_t next_level = (uint8_t)(frac * 20.0f + 0.5f);            // 0 -> 20
    uint8_t neighbor = forward ? (uint8_t)(base + 1) : (base == 0 ? 0 : (uint8_t)(base - 1));
    if (neighbor > end) neighbor = end;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        uint8_t level = 0;
        if (i == base) {
            level = head_level;
        } else if (i == neighbor && neighbor != base) {
            level = next_level;
        }
        put_pixel(rgb_to_grb(level, level, 0)); // warm yellow bounce with smooth cross-fade
    }

    state->last_pos = base;
    state->frame_idx++;
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
                        bool show_calories, bool paused, uint32_t now_ms,
                        uint32_t flash_until_ms) {
    oled_home();

    bool flash = now_ms < flash_until_ms;

    if (show_calories) {
        oled_print(6, 4, "CAL");
        oled_show_battery_animated(battery_percent, now_ms);
        oled_show_calories(calories);
    } else {
        oled_print(6, 4, "STEPS");
        oled_show_battery_animated(battery_percent, now_ms);
        oled_show_steps(steps);
    }

    if (paused) {
        // Show "PAUSED" near the bottom of the 32px display
        oled_print(32, 24, "PAUSED");
    }

    if (flash) {
        // 1px border flash to indicate a new step
        oled_draw_rect(0, 0, OLED_WIDTH, OLED_HEIGHT, 1);
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
    // Seed last button states from actual GPIO levels so we don't auto-toggle on boot.
    bool last_mode_level = gpio_get(BUTTON_MODE_PIN);
    uint32_t last_mode_toggle_ms = 0;  // debounce timer
    bool workout_running = true;       // start immediately
    bool workout_started = true;
    bool last_start_level = gpio_get(BUTTON_START_PIN);
    uint32_t last_start_toggle_ms = 0; // unused while auto-run is enabled
    uint32_t workout_steps = 0;
    uint32_t workout_offset = 0;       // start counting from boot
    uint32_t step_flash_until_ms = 0;  // border flash timer on step increment

    if (oled_ok) {
        const char *title = "Pico Activity Tracker";
        size_t title_len = strlen(title);
        uint16_t title_px = (uint16_t)(title_len * (FONT_WIDTH + 1));
        if (title_px > 0) title_px -= 1;
        int16_t target_x = (title_px < OLED_WIDTH) ? (int16_t)((OLED_WIDTH - title_px) / 2) : 0;
        uint16_t slide_frames = (uint16_t)(((OLED_WIDTH - target_x) / 2) + 1); // matches oled_slide_in_text step of 2px
        if (slide_frames < 2) slide_frames = 2;
        uint16_t travel_frames = slide_frames / 2; // two end-to-end travels during the slide
        if (travel_frames == 0) travel_frames = 1;

        uint8_t title_y = (OLED_HEIGHT - 8) / 2; // center the 8px font vertically
        led_bounce_state_t bounce = {.last_pos = 0, .frame_idx = 0, .frames_per_travel = travel_frames};
        oled_slide_in_text_hook(title, title_y, 12,
                                startup_led_bounce_step, &bounce);
        // Clear LEDs before handing control to the normal update loop
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            put_pixel(rgb_to_grb(0, 0, 0));
        }
        sleep_ms(400); // let the title sit briefly before normal UI resumes
    }

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

        // Auto-run: always count steps from boot; ignore start/pause button for now
        (void)last_start_level;
        (void)last_start_toggle_ms;
        (void)workout_started;
        (void)workout_running;

        if (total_steps >= workout_offset) {
            workout_steps = total_steps - workout_offset;
        } else {
            workout_steps = 0;
            workout_offset = total_steps;
        }

        uint32_t calories = steps_to_calories(workout_steps, USER_WEIGHT_LBS, USER_HEIGHT_CATEGORY);

        if (workout_steps != prev_steps) {
            printf("STEP %lu @ %lums\n", workout_steps, now_ms);
            prev_steps = workout_steps;
            step_flash_until_ms = now_ms + 180; // brief border flash
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

        update_led_bar(workout_steps, battery_percent, now_ms);

        if ((now_ms - last_display_ms) >= DISPLAY_REFRESH_MS) {
            last_display_ms = now_ms;
            bool paused = false; // always running in auto mode
            render_oled(workout_steps, calories, battery_percent, show_calories, paused, now_ms,
                        step_flash_until_ms);
            printf("steps=%lu cal=%lu soc=%.1f%% %s\n",
                   workout_steps, calories, soc, paused ? "[PAUSED]" : "");
        }

        sleep_ms(5);
    }
}
