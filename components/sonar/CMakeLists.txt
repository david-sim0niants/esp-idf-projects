idf_component_register(
    REQUIRES "driver" "esp_timer" "wifi_ap" "common"
    SRCS "sonar.c"
    INCLUDE_DIRS "include"
)
