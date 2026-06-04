/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * home.c
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

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "cta.h"

static const char *TAG = CTA_TAB_NAME;

void display_cta_tab( lv_obj_t *tv )
{
    lvgl_port_lock( 0 );
    
    lv_obj_t *cta_tab = ui_tabview_add_tab( tv, CTA_TAB_NAME );

    /* Card with flex-column layout */
    lv_obj_t *card = ui_create_card( cta_tab, lv_color_make( 35, 47, 62 ) );
    ui_card_title( card, "Next Steps", lv_color_make(255,255,255) );
    ui_card_text( card, "Get hands-on experience building IoT solutions and learn about the AWS IoT EduKit program:", lv_color_make(255,255,255) );

    /* URL label — flex grows to push it toward bottom */
    lv_obj_t *spacer = lv_obj_create( card );
    lv_obj_remove_style_all( spacer );
    lv_obj_set_size( spacer, 0, 0 );
    lv_obj_set_flex_grow( spacer, 1 );

    lv_obj_t *url_label = lv_label_create( card );
    lv_obj_set_style_text_color( url_label, lv_color_hex( UI_ACCENT_COLOR ), 0 );
    lv_label_set_text( url_label, "https://edukit.workshop.aws" );
    lv_obj_set_width( url_label, lv_pct( 100 ) );
    lv_obj_set_style_text_align( url_label, LV_TEXT_ALIGN_CENTER, 0 );
    
    lvgl_port_unlock();

    ESP_LOGD( TAG, "Building tab" );
}