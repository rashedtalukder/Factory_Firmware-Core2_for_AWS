/*
 * Screenshot capture over serial (development tool).
 * Remove this file when UI iteration is complete.
 *
 * Usage: Send 'S' (0x53) over UART to trigger a screenshot.
 * The firmware will respond with base64-encoded RGB565 pixel data
 * between start/end markers.
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

#include "screenshot.h"

static const char *TAG = "SCREENSHOT";

/* Base64 encoding table */
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Encode a block of binary data to base64 and print it line-by-line.
 * Outputs in 76-char lines per RFC 2045.
 */
static void base64_print(const uint8_t *data, size_t len)
{
    size_t i = 0;
    int line_pos = 0;
    char line_buf[80]; /* 76 chars + \r\n + \0 */

    while (i < len) {
        uint32_t n = (uint32_t)data[i++] << 16;
        if (i < len) n |= (uint32_t)data[i++] << 8;
        else { line_buf[line_pos++] = b64_table[(n >> 18) & 0x3F];
               line_buf[line_pos++] = b64_table[(n >> 12) & 0x3F];
               line_buf[line_pos++] = '=';
               line_buf[line_pos++] = '=';
               break; }
        if (i < len) n |= (uint32_t)data[i++];
        else { line_buf[line_pos++] = b64_table[(n >> 18) & 0x3F];
               line_buf[line_pos++] = b64_table[(n >> 12) & 0x3F];
               line_buf[line_pos++] = b64_table[(n >> 6) & 0x3F];
               line_buf[line_pos++] = '=';
               break; }

        line_buf[line_pos++] = b64_table[(n >> 18) & 0x3F];
        line_buf[line_pos++] = b64_table[(n >> 12) & 0x3F];
        line_buf[line_pos++] = b64_table[(n >> 6) & 0x3F];
        line_buf[line_pos++] = b64_table[n & 0x3F];

        if (line_pos >= 76) {
            line_buf[line_pos] = '\0';
            printf("%s\n", line_buf);
            line_pos = 0;
        }
    }

    if (line_pos > 0) {
        line_buf[line_pos] = '\0';
        printf("%s\n", line_buf);
    }
}

/**
 * Take a screenshot of the active LVGL screen and send it over UART.
 */
static void do_screenshot(void)
{
    ESP_LOGI(TAG, "Capturing screenshot...");

    lvgl_port_lock(0);

    lv_obj_t *scr = lv_screen_active();
    lv_draw_buf_t *snap = lv_snapshot_take(scr, LV_COLOR_FORMAT_RGB565);

    lvgl_port_unlock();

    if (snap == NULL || snap->data == NULL) {
        ESP_LOGE(TAG, "Snapshot failed — not enough memory");
        return;
    }

    uint32_t w = snap->header.w;
    uint32_t h = snap->header.h;
    uint32_t stride = snap->header.stride;
    size_t data_size = stride * h;

    ESP_LOGI(TAG, "Snapshot: %lux%lu, stride=%lu, size=%u bytes",
             w, h, stride, (unsigned)data_size);

    /* Print with markers so the host script can extract the data */
    printf("---SCREENSHOT_START---\n");
    printf("W:%lu\n", w);
    printf("H:%lu\n", h);
    printf("STRIDE:%lu\n", stride);
    printf("FMT:RGB565_SWAP\n");

    base64_print(snap->data, data_size);

    printf("---SCREENSHOT_END---\n");
    fflush(stdout);

    ESP_LOGI(TAG, "Screenshot sent (%u bytes encoded)", (unsigned)data_size);

    lv_draw_buf_destroy(snap);
}

/**
 * Task that listens for the 'S' character on UART0 stdin.
 */
static void screenshot_task(void *pvParameters)
{
    /* Configure UART0 RX so we can read single chars */
    uart_config_t uart_conf = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    /* Only install driver if not already installed */
    if (!uart_is_driver_installed(UART_NUM_0)) {
        uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
        uart_param_config(UART_NUM_0, &uart_conf);
    }

    uint8_t byte;
    for (;;) {
        int len = uart_read_bytes(UART_NUM_0, &byte, 1, portMAX_DELAY);
        if (len > 0 && byte == 'S') {
            do_screenshot();
        }
    }
}

void screenshot_take(void)
{
    do_screenshot();
}

void screenshot_init(void)
{
    xTaskCreatePinnedToCore(screenshot_task, "screenshot", 8192, NULL, 2, NULL, 0);
    ESP_LOGI(TAG, "Screenshot listener started — send 'S' to capture");
}
