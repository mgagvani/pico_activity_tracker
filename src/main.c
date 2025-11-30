#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

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

    // debug
    // while (true) {
    //     printf("Hello World!\n");
    // }

    // setup I2C 
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // (void)quickstart();

    while (true) {
        float vbat = read_voltage();
        float soc = read_soc();

        printf("VBAT: %.3f V | SOC: %.2f %%\n", vbat, soc);
        sleep_ms(100);
    }
}
