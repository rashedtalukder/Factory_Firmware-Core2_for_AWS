/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * crypto.c
 * 
 * Copyright (C) 2022 Rashed Talukder. All Rights Reserved.
 * Copyright (C) 2022 M5Stack. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "crypto.h"

static const char *TAG = CRYPTO_TAB_NAME;

void display_crypto_tab( lv_obj_t *tv )
{
    ESP_LOGD( TAG, "Building tab" );
    lvgl_port_lock( 0 );

    lv_obj_t *crypto_tab = ui_tabview_add_tab( tv, CRYPTO_TAB_NAME );

    /* Card with flex-column layout */
    lv_obj_t *card = ui_create_card( crypto_tab, lv_color_make( 4, 151, 150 ) );
    ui_card_title( card, "ATECC608 Crypto-Auth", lv_color_make(0,0,0) );
    ui_card_text( card, "The ATECC608 stores device certificates and signs with ECDSA.", lv_color_make(0,0,0) );

    lv_obj_t *serial_row = ui_card_action( card, "Device serial number:", lv_color_make(0,0,0), LV_FLEX_ALIGN_CENTER );
    lv_obj_t *serial_label = ui_value_label( serial_row, "Reading..." );

    lvgl_port_unlock();

    char *device_serial = heap_caps_malloc( CRYPTO_SERIAL_STR_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM );
    esp_err_t ret = device_serial ? core2foraws_crypto_serial_get( device_serial ) : ESP_ERR_NO_MEM;
    if ( ret == ESP_OK )
        ESP_LOGD( TAG, "Secure element serial: %s", device_serial );
    else
        ESP_LOGE( TAG, "Secure element failure: %s", esp_err_to_name( ret ) );

    if ( lvgl_port_lock( 1000 ) )
    {
        lv_label_set_text( serial_label, ret == ESP_OK ? device_serial : "Unavailable" );
        lvgl_port_unlock();
    }

    heap_caps_free( device_serial );
}