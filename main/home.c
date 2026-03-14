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

#include "home.h"

static const char *TAG = HOME_TAB_NAME;

void display_home_tab( lv_obj_t *tv )
{
    lvgl_port_lock( 0 );
    
    lv_obj_t *home_tab = lv_tabview_add_tab( tv, HOME_TAB_NAME );
    lv_obj_set_style_pad_all( home_tab, 0, 0 );

    /* Create the title within the tab */
    static lv_style_t title_style;
    lv_style_init( &title_style );
    lv_style_set_text_font( &title_style, LV_FONT_DEFAULT );
    lv_style_set_text_color( &title_style, lv_color_make(0,0,0) );
    
    lv_obj_t *tab_title_label = lv_label_create( home_tab );
    lv_obj_add_style( tab_title_label, &title_style, 0 );
    lv_label_set_text_static( tab_title_label, "M5Stack\nCore2 for AWS IoT EduKit" );
    lv_obj_set_style_text_align( tab_title_label, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_align( tab_title_label, LV_ALIGN_TOP_MID, 0, 50 );

    lv_obj_t *body_label = lv_label_create( home_tab );
    lv_label_set_long_mode( body_label, LV_LABEL_LONG_WRAP );
    lv_label_set_text_static( body_label, "Swipe through to learn about some of the hardware features." );
    lv_obj_set_width( body_label, 280 );
    lv_obj_align( body_label, LV_ALIGN_CENTER, 0 , 10 );
    
    lv_obj_t *arrow_label = lv_label_create( home_tab );
    lv_label_set_text( arrow_label,
        LV_SYMBOL_LEFT "       " LV_SYMBOL_LEFT "       " LV_SYMBOL_LEFT "       SWIPE      "
        LV_SYMBOL_LEFT "       " LV_SYMBOL_LEFT "       " LV_SYMBOL_LEFT "       SWIPE" );
    lv_label_set_long_mode( arrow_label, LV_LABEL_LONG_SCROLL_CIRCULAR );
    lv_obj_set_style_anim_duration( arrow_label, 6000, 0 );
    lv_obj_set_size( arrow_label, 290, 20 );
    lv_obj_set_style_text_align( arrow_label, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_align( arrow_label, LV_ALIGN_BOTTOM_MID, 0 , -40 );
    lvgl_port_unlock();
    
    ESP_LOGI( TAG, "\n\nWelcome to your M5Stack Core2 for AWS IoT EduKit reference hardware! Visit https://edukit.workshop.aws to view the tutorials and start learning how to build IoT solutions using AWS services.\n\n" );
}