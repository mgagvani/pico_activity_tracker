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

    // Display test text on OLED
    oled_print(0, 0, "Hello World!");
    oled_print(0, 10, "Pico Activity");
    oled_print(0, 20, "Tracker v1.0");
    oled_print(0, 40, "ECE 362 Project");
    oled_display();

    // (void)quickstart();

    while (true) {
        float vbat = read_voltage();
        float soc = read_soc();

        printf("VBAT: %.3f V | SOC: %.2f %%\n", vbat, soc);

        // Update OLED with battery info
        char buf[32];
        
        // Clear the bottom portion for updating values
        oled_fill_rect(0, 52, OLED_WIDTH, 12, 0);
        
        snprintf(buf, sizeof(buf), "BAT: %.2fV  %.0f%%", vbat, soc);
        oled_print(0, 52, buf);
        oled_display();

        sleep_ms(1000);
    }
}
