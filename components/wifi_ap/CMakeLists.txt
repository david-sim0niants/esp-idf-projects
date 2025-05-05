idf_component_register(
    REQUIRES "esp_wifi" "esp_netif" "nvs_flash"
    SRCS "wifi_ap.c"
    INCLUDE_DIRS "include"
)
