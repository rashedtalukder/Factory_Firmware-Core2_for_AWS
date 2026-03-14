/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * crypto.c
 * 
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "core2foraws.h"

#include "crypto.h"

static const char *TAG = CRYPTO_TAB_NAME;

void display_crypto_tab( lv_obj_t *tv )
{
    lvgl_port_lock( 0 );

    lv_obj_t *crypto_tab = lv_tabview_add_tab( tv, CRYPTO_TAB_NAME );
    lv_obj_set_style_pad_all( crypto_tab, 0, 0 );

    /* Create the main body object and set background within the tab*/
    static lv_style_t bg_style;
    lv_obj_t *crypto_bg = lv_obj_create( crypto_tab );
    lv_obj_set_style_pad_all( crypto_bg, 0, 0 );
    lv_obj_align( crypto_bg, LV_ALIGN_TOP_LEFT, 16, 36 );
    lv_obj_set_size( crypto_bg, 290, 190 );
    lv_obj_remove_flag( crypto_bg, LV_OBJ_FLAG_CLICKABLE );
    lv_style_init( &bg_style );
    lv_style_set_bg_color( &bg_style, lv_color_make( 4, 151, 150 ) );
    lv_obj_add_style( crypto_bg, &bg_style, 0 );

    /* Create the title within the main body object */
    static lv_style_t title_style;
    lv_style_init( &title_style );
    lv_style_set_text_font( &title_style, LV_FONT_DEFAULT );
    lv_style_set_text_color( &title_style, lv_color_make(0,0,0) );
    lv_obj_t *tab_title_label = lv_label_create( crypto_bg );
    lv_obj_add_style( tab_title_label, &title_style, 0 );
    lv_label_set_text_static( tab_title_label, "ATECC608 Crypto-Auth" );
    lv_obj_align( tab_title_label, LV_ALIGN_TOP_MID, 0, 10 );

    /* Create the sensor information label object */
    lv_obj_t *body_label = lv_label_create( crypto_bg );
    lv_label_set_long_mode( body_label, LV_LABEL_LONG_WRAP );
    lv_label_set_text_static( body_label, "The ATECC608 comes with pre-provisioned static certificates, along with Elliptic Curve Digital Signature Algorithm (ECDSA) sign/verify capability." );
    lv_obj_set_width( body_label, 252 );
    lv_obj_align_to( body_label, crypto_bg, LV_ALIGN_TOP_LEFT, 20, 40 );

    static lv_style_t body_style;
    lv_style_init( &body_style );
    lv_style_set_text_color( &body_style, lv_color_make(0,0,0) );
    lv_obj_add_style( body_label, &body_style, 0 );

    lvgl_port_unlock();

    char *device_serial = heap_caps_malloc( CRYPTO_SERIAL_STR_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM );
    esp_err_t ret = core2foraws_crypto_serial_get( device_serial );
    if ( ret == ESP_OK )
    {
        char sn_pretext[] = "Serial  # ";
        size_t sn_pretext_len = strlen( sn_pretext );
        char sn_label_text[ CRYPTO_SERIAL_STR_SIZE + sn_pretext_len - 1 ];
        snprintf( sn_label_text, CRYPTO_SERIAL_STR_SIZE + sn_pretext_len - 1, "%s%s", sn_pretext, device_serial );
        lvgl_port_lock( 0 );
        lv_obj_t *serial_label = lv_label_create( crypto_bg );
        lv_label_set_text( serial_label, sn_label_text );
        lv_obj_set_style_text_align( serial_label, LV_TEXT_ALIGN_CENTER, 0 );
        lv_obj_align( serial_label, LV_ALIGN_BOTTOM_MID, 0, -14 );
        lv_obj_add_style( serial_label, &body_style, 0 );
        lvgl_port_unlock();
        heap_caps_free( device_serial );
    }
    else
    {
        ESP_LOGE( TAG, "Secure element failure. Error code: %d", ret );
    }
}