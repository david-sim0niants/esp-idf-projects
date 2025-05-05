#include "common/http_server.h"

#include <stdlib.h>

static httpd_handle_t http_server = NULL;

esp_err_t start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    esp_err_t e = httpd_start(&server, &config);
    if (e == ESP_OK)
        http_server = server;
    return e;
}

void stop_http_server(void)
{
    if (http_server_running()) {
        httpd_stop(http_server);
    }
}

httpd_handle_t get_http_server(void)
{
    if (! http_server_running()) {
        ESP_ERROR_CHECK(start_http_server());
        if (http_server_running())
            atexit(stop_http_server);
    }
    return http_server;
}

bool http_server_running(void)
{
    return !!http_server;
}
