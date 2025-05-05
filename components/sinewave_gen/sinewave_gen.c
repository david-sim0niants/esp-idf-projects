#include "sinewave_gen.h"

#include <stdio.h>
#include <math.h>

#include <driver/gpio.h>
#include <driver/dac.h>
#include <freertos/FreeRTOS.h>

void sinewave_gen(void)
{
    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

    dac_output_enable(DAC_CHAN_0);
    double phase = 0;

#define FREQ 25

    while (1) {
        int voltage_diff = sin(phase) * 128;
        if (voltage_diff >= 128)
            voltage_diff = 127;
        else if (voltage_diff < -128)
            voltage_diff = -128;
        uint8_t voltage = 128 + voltage_diff;
        printf("%u\n", voltage);

        dac_output_voltage(DAC_CHAN_0, voltage);
        vTaskDelay(1);

        int period_ms = portTICK_PERIOD_MS;
        phase += 2 * M_PI * FREQ * period_ms / 1000.0;
        printf("%d %f\n", period_ms, phase);
        if (phase > 2 * M_PI)
            phase -= 2 * M_PI;
    }
}
