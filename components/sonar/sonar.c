#include "sonar.h"

#include <stdatomic.h>
#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <driver/dac_oneshot.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_log.h>

#include "common/http_server.h"

#define TAG "sonar"

static void init();
static void deinit();

static void enable_device_comm();
static void disable_device_comm();

#define SONAR_TRIGGER_CHANNEL DAC_CHAN_0
#define SONAR_RX_PIN GPIO_NUM_15

#define SPEED_OF_SOUND_CM_PER_US 0.0343

static void sonar_echo_isr_handler(void *arg);
static dac_oneshot_handle_t trigger_handle;

static void enable_http_uri();
static void disable_http_uri();

static esp_err_t http_get_handler(httpd_req_t *req);
static httpd_uri_t uri_get = {
    .uri      = "/" TAG "/distance",
    .method   = HTTP_GET,
    .handler  = http_get_handler,
    .user_ctx = NULL,
};

static long get_pulse_width_us();
static void set_pulse_width_us(long value);
static atomic_long pulse_width_us = 0;

void sonar_main(void)
{
    init();

    while (1) {
        ESP_ERROR_CHECK(dac_oneshot_output_voltage(trigger_handle, 255));
        esp_rom_delay_us(10);
        ESP_ERROR_CHECK(dac_oneshot_output_voltage(trigger_handle, 0));

        vTaskDelay(10);

        long sonic_time = atomic_load_explicit(&pulse_width_us, memory_order_acquire);
        double distance = sonic_time * SPEED_OF_SOUND_CM_PER_US / 2;
        ESP_LOGI(TAG, "Distance: %fcm", distance);
    }

    deinit();
}

static void init()
{
    enable_http_uri();
    enable_device_comm();
}

static void deinit()
{
    disable_device_comm();
    disable_http_uri();
}

static void enable_http_uri()
{
    ESP_ERROR_CHECK(
            httpd_register_uri_handler(get_http_server(), &uri_get));
}

static void disable_http_uri()
{
    ESP_ERROR_CHECK(
            httpd_unregister_uri_handler(get_http_server(), uri_get.uri, uri_get.method));
}

static void enable_device_comm()
{
    gpio_config_t rx_config = {
        .pin_bit_mask = (uint64_t)1 << SONAR_RX_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&rx_config));
    ESP_ERROR_CHECK(gpio_isr_handler_add(SONAR_RX_PIN, sonar_echo_isr_handler, NULL));

    dac_oneshot_config_t trigger_config = {SONAR_TRIGGER_CHANNEL};
    ESP_ERROR_CHECK(dac_oneshot_new_channel(&trigger_config, &trigger_handle));
}

static void disable_device_comm()
{
    ESP_ERROR_CHECK(gpio_isr_handler_remove(SONAR_RX_PIN));
    ESP_ERROR_CHECK(dac_oneshot_del_channel(trigger_handle));
}

static void sonar_echo_isr_handler(void *arg)
{
    static long rise_time = 0;

    if (gpio_get_level(SONAR_RX_PIN)) {
        rise_time = esp_timer_get_time();
    } else {
        uint64_t fall_time = esp_timer_get_time();
        set_pulse_width_us(fall_time - rise_time);
    }
}

static esp_err_t http_get_handler(httpd_req_t *req)
{
    char response[32];
    long sonic_time = get_pulse_width_us();
    sprintf(response, "%f", sonic_time * SPEED_OF_SOUND_CM_PER_US / 2);

    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static long get_pulse_width_us()
{
    return atomic_load_explicit(&pulse_width_us, memory_order_acquire);
}

static void set_pulse_width_us(long value)
{
    atomic_store_explicit(&pulse_width_us, value, memory_order_release);
}
