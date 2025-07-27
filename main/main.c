#include <string.h>

#include <driver/gpio.h>

// #include "sonar.h"
#include "wifi_ap.h"
#include "st7735s-driver.h"

void app_main(void)
{
    wifi_ap_config_t ap_config;
    memset(&ap_config, 0, sizeof(ap_config));

    const char ssid[] = "Sonar Control AP";
    const char password[] = "ultrasonic";

    memcpy(ap_config.ssid, ssid, sizeof(ssid));;
    memcpy(ap_config.password, password, sizeof(password));;
    ap_config.ssid_len = 0;
    ap_config.channel = 0;
    ap_config.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ssid_hidden = 0;
    ap_config.max_connection = 16;
    ap_config.beacon_interval = 100;

    start_wifi_ap(&ap_config);

    st7735s_init();
    st7735s_display_random();
    st7735s_deinit();

    // gpio_install_isr_service(0);
    // sonar_main();
    // gpio_uninstall_isr_service();

    stop_wifi_ap();
}
