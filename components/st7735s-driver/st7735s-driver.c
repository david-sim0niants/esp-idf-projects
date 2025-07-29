#include <stdio.h>

#include "st7735s-driver.h"
#include "min_max.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "freertos/idf_additions.h"

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
} handle = {
    .spi = NULL,
    .cur_win = {{0, 0}, {0, 0}},
    .cur_colmod = st7735s_COLMOD_NONE
};

void st7735s_init(void)
{
    spi_bus_config_t bus_config = {
        .miso_io_num = -1,
        .mosi_io_num = PIN_SDA,
        .sclk_io_num = PIN_SCL,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
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

static void send_word(uint16_t word)
{
    char bytes[2] = {word >> 8, word & 7};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = bytes,
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

static void send_data(const void *data, size_t size)
{
    gpio_set_level(PIN_DC, DATA_ACTIVE);

    const int chunk_size = 4092;

    for (size_t tx_size = 0; tx_size < size; tx_size += chunk_size) {
        spi_transaction_t t = {
            .length = MIN(chunk_size, size - tx_size) * 8,
            .tx_buffer = data,
            .flags = 0,
        };
        ESP_ERROR_CHECK(spi_device_transmit(handle.spi, &t));
    }
}

static uint8_t get_colmod_code(st7735s_ColorMode colmod)
{
    switch (colmod) {
    case st7735s_COLMOD_NONE:
        return 5;
    default:
        return -1;
    }
}

void st7735s_display_random(void)
{
    st7735s_set_color_mode(st7735s_RGB565);
    st7735s_AddressWindow win = {
        .rows = {
            .start = 0,
            .end = 128,
        },
        .cols = {
            .start = 0,
            .end = 160,
        }
    };
    size_t nr_pixels = 160 * 128;
    uint16_t *buffer = malloc(nr_pixels * 2);
    for (int i = 0; i < nr_pixels; ++i)
        buffer[i] = 0x55AA;
    while (true) {
        st7735s_draw(win, (uint8_t *)buffer);
        vTaskDelay(portTICK_PERIOD_MS * 2);
    }
}

esp_err_t st7735s_set_color_mode(st7735s_ColorMode mode)
{
    uint8_t colmod_code = get_colmod_code(mode);
    if (colmod_code == 0)
        return ESP_ERR_INVALID_ARG;

    handle.cur_colmod = mode;
    send_cmd(CMD_COLMOD);
    send_data_byte(colmod_code);

    return ESP_OK;
}

void st7735s_draw(st7735s_AddressWindow window, const void *data)
{
    st7735s_set_window(&window);
    st7735s_write_data(data);
}

void st7735s_set_window(st7735s_AddressWindow *window)
{
    st7735s_set_ras(window->rows);
    st7735s_set_cas(window->cols);
}

void st7735s_write_data(const void *data)
{
    send_data(data, st7735s_get_current_expected_data_size());
}

static void send_address_set(st7735s_AddressSet as)
{
    send_word(as.start);
    send_word(as.end);
}

void st7735s_set_ras(st7735s_AddressSet ras)
{
    assert(st7735s_is_valid_address_set(ras));
    handle.cur_win.rows = ras;
    send_cmd(CMD_RASET);
    send_address_set(ras);
}

void st7735s_set_cas(st7735s_AddressSet cas)
{
    assert(st7735s_is_valid_address_set(cas));
    handle.cur_win.cols = cas;
    send_cmd(CMD_CASET);
    send_address_set(cas);
}

size_t st7735s_get_current_expected_data_size()
{
    return st7735s_calc_expected_bufsize(&handle.cur_win, handle.cur_colmod);
}
