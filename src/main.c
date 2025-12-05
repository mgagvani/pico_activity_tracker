#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "oled.h"

// functions
float read_voltage(void);
float read_soc(void);
int quickstart(void);

// copied from max17048.c
#define I2C_PORT    i2c1
#define I2C_SDA_PIN 10
#define I2C_SCL_PIN 11

int main() {
    stdio_init_all();

    // setup I2C 
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Initialize OLED display
    if (!oled_init()) {
        printf("OLED init failed!\n");
    }

    // (void)quickstart();

    uint32_t steps = 0;  // TODO: get actual steps from IMU

    while (true) {
        float soc = read_soc();
        uint8_t battery_percent = (soc < 0) ? 0 : (soc > 100) ? 100 : (uint8_t)soc;

        printf("Steps: %lu | SOC: %.1f%%\n", steps, soc);

        // Clear and reset cursor
        oled_home();

        // Draw UI using convenience functions
        oled_print(4, 2, "STEPS");     // Label top-left
        oled_show_battery(battery_percent);  // Battery icon top-right (auto-positioned)
        oled_show_steps(steps);              // Large centered step count (auto-centered)

        oled_display();

        // Simulate step counting (remove this when using real IMU)
        steps += 1;

        sleep_ms(500);
    }
}
