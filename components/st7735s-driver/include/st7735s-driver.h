#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#include "esp_err.h"

typedef enum {
    st7735s_COLMOD_NONE,
    st7735s_RGB565,
} st7735s_ColorMode;

typedef struct {
    uint16_t start, end;
} st7735s_AddressSet;

static inline bool st7735s_is_valid_address_set(st7735s_AddressSet as)
{
    return as.start <= as.end;
}

typedef struct {
    st7735s_AddressSet rows, cols;
} st7735s_AddressWindow;

static inline bool st7735s_is_valid_address_window(st7735s_AddressWindow *window)
{
    return st7735s_is_valid_address_set(window->rows) && st7735s_is_valid_address_set(window->cols);
}

void st7735s_init(void);
void st7735s_deinit(void);
void st7735s_display_random(void);

esp_err_t st7735s_reset();
esp_err_t st7735s_set_color_mode(st7735s_ColorMode mode);
void st7735s_draw(st7735s_AddressWindow window, const void *data);

void st7735s_set_window(st7735s_AddressWindow *window);
void st7735s_write_data(const void *data);

void st7735s_set_cas(st7735s_AddressSet cas);
void st7735s_set_ras(st7735s_AddressSet ras);

size_t st7735s_get_current_expected_data_size(void);

static inline size_t st7735s_get_window_cell_count(st7735s_AddressWindow *window)
{
    assert(st7735s_is_valid_address_window(window));
    return (window->rows.end - window->rows.start + 1) * (window->cols.end - window->cols.start + 1);
}

static inline size_t st7735s_calc_expected_bufsize(st7735s_AddressWindow *window, st7735s_ColorMode mode)
{
    switch (mode) {
    case st7735s_COLMOD_NONE:
        return 0;
    case st7735s_RGB565:
        return 2 * st7735s_get_window_cell_count(window);
    default:
        abort();
    }
}
