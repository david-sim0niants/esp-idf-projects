#include <stdio.h>
#include "st7735s-driver.h"

#include <driver/gpio.h>
#include <driver/spi_common.h>

void st7735s_init(void)
{
    spi_bus_config_t config = {
        .miso_io_num = -1,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_CLK,
    };
    spi_bus_initialize(HSPI_HOST, , spi_dma_chan_t dma_chan)
}
