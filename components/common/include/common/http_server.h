#pragma once

#include <esp_http_server.h>
#include <stdbool.h>

esp_err_t start_http_server(void);
void stop_http_server(void);
httpd_handle_t get_http_server(void);
bool http_server_running();
