#include <stdio.h>

#include "st7735s-driver.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"

#define PIN_SDA GPIO_NUM_13
#define PIN_SCL GPIO_NUM_14
#define PIN_CS  GPIO_NUM_32
#define PIN_DC  GPIO_NUM_25

#define COMM_ACTIVE 0
#define DATA_ACTIVE 1

#define CMD_COLMOD 0x3A
#define CMD_RASET 0x2B
#define CMD_CASET 0x2A

// TODO: PIN_RST

static struct {
    spi_device_handle_t spi;
    st7735s_AddressWindow cur_win;
    st7735s_ColorMode cur_colmod;
} handle;

void st7735s_init(void)
{
    spi_bus_config_t bus_config = {
        .miso_io_num = -1,
        .mosi_io_num = PIN_SDA,
        .sclk_io_num = PIN_SCL,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &bus_config, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &dev_config, &handle.spi));

    gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
    // TODO: manage PIN_RST
}

void st7735s_deinit(void)
{
    spi_bus_remove_device(handle.spi);
    spi_bus_free(HSPI_HOST);
}

static void send_byte(uint8_t byte)
{
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &byte,
    };
    ESP_ERROR_CHECK(spi_device_transmit(handle.spi, &t));
}

static void send_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_DC, COMM_ACTIVE);
    send_byte(cmd);
}

static void send_data_byte(uint8_t byte)
{
    ESP_ERROR_CHECK(gpio_set_level(PIN_DC, DATA_ACTIVE));
    send_byte(byte);
}

static void send_data(uint8_t *data, size_t size)
{
    gpio_set_level(PIN_DC, DATA_ACTIVE);
    spi_transaction_t t = {
        .length = size * 8,
        .tx_buffer = data,
    };
    ESP_ERROR_CHECK(spi_device_transmit(handle.spi, &t));
}

static uint8_t get_colmod_code(st7735s_ColorMode colmod)
{
    switch (colmod) {
    case st7735s_RGB565:
        return 5;
    default:
        return -1;
    }
}

void st7735s_display_random(void)
{
    // TODO
}

esp_err_t st7735s_set_color_mode(st7735s_ColorMode mode)
{
    uint8_t colmod_code = get_colmod_code(mode);
    if (colmod_code == 0)
        return ESP_ERR_INVALID_ARG;

    send_cmd(CMD_COLMOD);
    send_data_byte(colmod_code);

    return ESP_OK;
}

void st7735s_draw(st7735s_AddressWindow window, uint8_t *data)
{
    st7735s_set_window(&window);
    st7735s_write_buffer(data);
}

void st7735s_set_window(st7735s_AddressWindow *window)
{
    st7735s_set_ras(window->rows);
    st7735s_set_cas(window->cols);
}

void st7735s_write_buffer(uint8_t *buffer)
{
    send_data(buffer, st7735s_get_current_expected_data_size());
}

static void send_address_set(st7735s_AddressSet as)
{
    assert(is_valid_address_set(as));
    send_byte(as.start >> 8);
    send_byte(as.start & 7);
    send_byte(as.end >> 8);
    send_byte(as.end & 7);
}

void st7735s_set_ras(st7735s_AddressSet ras)
{
    handle.cur_win.rows = ras;
    send_cmd(CMD_RASET);
    send_address_set(ras);
}

void st7735s_set_cas(st7735s_AddressSet cas)
{
    handle.cur_win.cols = cas;
    send_cmd(CMD_CASET);
    send_address_set(cas);
}

size_t st7735s_get_current_expected_data_size()
{
    return st7735s_calc_expected_bufsize(&handle.cur_win, handle.cur_colmod);
}
