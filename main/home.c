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
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "home.h"

static const char *TAG = HOME_TAB_NAME;

void display_home_tab( lv_obj_t *tv )
{
    ESP_LOGD( TAG, "Building tab" );
    lvgl_port_lock( 0 );
    
    lv_obj_t *home_tab = ui_tabview_add_tab( tv, HOME_TAB_NAME );

    /* Home uses transparent card-like flex column layout directly on tab */
    lv_obj_set_flex_align( home_tab, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_row( home_tab, 10, 0 );

    /* Title */
    lv_obj_t *tab_title_label = lv_label_create( home_tab );
    lv_label_set_text_static( tab_title_label, "M5Stack\nCore2 for AWS IoT Kit" );
    lv_obj_set_style_text_color( tab_title_label, lv_color_make(255,255,255), 0 );
    lv_obj_set_style_text_align( tab_title_label, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_set_width( tab_title_label, lv_pct( 90 ) );

    /* Description */
    lv_obj_t *body_label = lv_label_create( home_tab );
    lv_obj_set_style_text_color( body_label, lv_color_hex( 0xaaaaaa ), 0 );
    lv_label_set_long_mode( body_label, LV_LABEL_LONG_WRAP );
    lv_label_set_text_static( body_label, "Swipe through to learn about some of the hardware features." );
    lv_obj_set_width( body_label, lv_pct( 85 ) );
    lv_obj_set_style_text_align( body_label, LV_TEXT_ALIGN_CENTER, 0 );
    
    /* Animated swipe hint — scrolls left continuously */
    lv_obj_t *arrow_label = lv_label_create( home_tab );
    lv_obj_set_style_text_color( arrow_label, lv_color_hex( UI_ACCENT_COLOR ), 0 );
    lv_obj_set_style_text_opa( arrow_label, LV_OPA_80, 0 );

    lv_label_set_text_static( arrow_label,
        LV_SYMBOL_LEFT "      Swipe to explore      "
        LV_SYMBOL_LEFT "      Swipe to explore      "
    );

    lv_label_set_long_mode( arrow_label, LV_LABEL_LONG_SCROLL_CIRCULAR );
    lv_obj_set_width( arrow_label, lv_pct( 90 ) );
    lv_obj_set_style_anim_duration( arrow_label, 8500, 0 );

    lvgl_port_unlock();
    
    ESP_LOGI( TAG, "\n\nWelcome to your M5Stack Core2 for AWS IoT EduKit reference hardware! Visit https://edukit.workshop.aws to view the tutorials and start learning how to build IoT solutions using AWS services.\n\n" );
}