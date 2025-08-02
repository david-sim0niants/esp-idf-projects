#include <stdio.h>

#include "st7735s-driver.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"

#define CLK_HZ (20 * 1000 * 1000)

#define PIN_SDA GPIO_NUM_13
#define PIN_SCL GPIO_NUM_14
#define PIN_RST GPIO_NUM_26
#define PIN_CS  GPIO_NUM_32
#define PIN_DC  GPIO_NUM_25

#define DC_COMM 0
#define DC_DATA 1

#define DATA_MAX_TRANSFER_SIZE 4096
#define DATA_TRANSFER_CHUNK_SIZE 4092

#define CMD_COLMOD 0x3A
#define CMD_RASET 0x2B
#define CMD_CASET 0x2A
#define CMD_RAMWR 0x2C

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
        .max_transfer_sz = DATA_MAX_TRANSFER_SIZE,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &bus_config, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = CLK_HZ,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &dev_config, &handle.spi));

    ESP_ERROR_CHECK(gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT));
}

void st7735s_deinit(void)
{
    spi_bus_remove_device(handle.spi);
    spi_bus_free(HSPI_HOST);
}

static void spi_transfer(const void *data, size_t length, uint32_t flags)
{
    spi_transaction_t t = {
        .length = length,
        .tx_buffer = data,
        .flags = flags,
    };
    ESP_ERROR_CHECK(spi_device_transmit(handle.spi, &t));
}

static inline void set_command()
{
    ESP_ERROR_CHECK(gpio_set_level(PIN_DC, DC_COMM));
}

static inline void set_data()
{
    ESP_ERROR_CHECK(gpio_set_level(PIN_DC, DC_DATA));
}

static void send_cmd(uint8_t cmd)
{
    set_command();
    spi_transfer(&cmd, 8, SPI_TRANS_CS_KEEP_ACTIVE);
}

static void send_data(const void *data, const size_t size)
{
    set_data();
    for (size_t tx_size = 0; tx_size < size; tx_size += DATA_TRANSFER_CHUNK_SIZE) {
        if (tx_size + DATA_TRANSFER_CHUNK_SIZE < size)
            spi_transfer(data + tx_size, DATA_TRANSFER_CHUNK_SIZE * 8, SPI_TRANS_CS_KEEP_ACTIVE);
        else
            spi_transfer(data + tx_size, (size - tx_size) * 8, 0);
    }
}

static inline void run(uint8_t cmd, const void *data, size_t length)
{
    spi_device_acquire_bus(handle.spi, portMAX_DELAY);
    send_cmd(cmd);
    send_data(data, length);
    spi_device_release_bus(handle.spi);
}

static inline uint8_t get_colmod_code(st7735s_ColorMode colmod)
{
    switch (colmod) {
    case st7735s_COLMOD_NONE:
        return 0;
    case st7735s_RGB565:
        return 5;
    default:
        abort();
        __builtin_unreachable();
    }
}

void st7735s_display_random(void)
{
    st7735s_AddressWindow win = {
        .rows = {
            .start = 0,
            .end = 8,
        },
        .cols = {
            .start = 0,
            .end = 16,
        }
    };
    size_t nr_pixels = 16 * 8;
    uint16_t *buffer = malloc(nr_pixels * 2);
    for (int i = 0; i < nr_pixels; ++i)
        buffer[i] = 0xF800;
    while (true) {
        st7735s_reset();
        st7735s_set_color_mode(st7735s_RGB565);
        st7735s_draw(win, (uint8_t *)buffer);
        vTaskDelay(1);
    }
}

esp_err_t st7735s_reset()
{
    esp_err_t e = gpio_set_level(PIN_RST, 0);
    if (e != ESP_OK)
        return e;
    vTaskDelay(pdMS_TO_TICKS(10));

    e = gpio_set_level(PIN_RST, 1);
    if (e != ESP_OK)
        return e;
    vTaskDelay(pdMS_TO_TICKS(10));

    return ESP_OK;
}

esp_err_t st7735s_set_color_mode(st7735s_ColorMode mode)
{
    uint8_t colmod_code = get_colmod_code(mode);
    if (colmod_code == 0)
        return ESP_ERR_INVALID_ARG;

    handle.cur_colmod = mode;
    run(CMD_COLMOD, &colmod_code, 1);

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
    run(CMD_RAMWR, data, st7735s_get_current_expected_data_size());
}

static void run_aset_cmd(char type, st7735s_AddressSet as)
{
    uint8_t data[] = {
        as.start >> 8,
        as.start & 0xFF,
        as.end >> 8,
        as.end & 0xFF,
    };
    run(type == 'r' ? CMD_RASET : CMD_CASET, data, 4);
}

void st7735s_set_ras(st7735s_AddressSet ras)
{
    assert(st7735s_is_valid_address_set(ras));
    handle.cur_win.rows = ras;
    run_aset_cmd('r', ras);
}

void st7735s_set_cas(st7735s_AddressSet cas)
{
    assert(st7735s_is_valid_address_set(cas));
    handle.cur_win.cols = cas;
    run_aset_cmd('c', cas);
}

size_t st7735s_get_current_expected_data_size(void)
{
    return st7735s_calc_expected_bufsize(&handle.cur_win, handle.cur_colmod);
}
