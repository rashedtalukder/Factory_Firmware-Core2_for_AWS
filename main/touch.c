/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * touch.c
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
#include "touch.h"

static const char *TAG = TOUCH_TAB_NAME;

// Should create a struct to pass pointers to events, but globals are easier to understand.
static uint8_t r = 0, g = 70, b = 79;
static lv_style_t bg_style;
static lv_obj_t *touch_bg;
static lv_obj_t *button_touch_label;

static void touch_button_callback( enum core2foraws_button_btns button, press_event_t event );

void display_touch_tab( lv_obj_t *tv )
{
    ESP_LOGD( TAG, "Building tab" );
    lvgl_port_lock( 0 );

    lv_obj_t *touch_tab = ui_tabview_add_tab( tv, TOUCH_TAB_NAME );

    /* Card — bg color is updated dynamically via touch callbacks */
    touch_bg = ui_create_card( touch_tab, lv_color_make( r, g, b ) );
    /* Store the initial bg_style for dynamic updates */
    lv_style_init( &bg_style );
    lv_style_set_bg_color( &bg_style, lv_color_make( r, g, b ) );
    lv_obj_add_style( touch_bg, &bg_style, 0 );

    ui_card_title( touch_bg, "FT6336U Capacitive Touch", lv_color_make(255,255,255) );
    ui_card_text( touch_bg, "The FT6336U is a capacitive touch panel controller that provides X and Y coordinates for touch input."
        "\n\n\n\nPress the touch buttons below.", lv_color_make(255,255,255) );

    button_touch_label = lv_label_create( touch_bg );
    lv_label_set_text( button_touch_label, "No button tapped" );
    lv_obj_set_style_text_align( button_touch_label, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_set_width( button_touch_label, lv_pct( 100 ) );

    /* Touch button indicator lines — flex row at bottom of tab */
    static lv_point_precise_t line_points[] = { {20, 0}, {70, 0} };

    static lv_style_t red_line_style;
    lv_style_init( &red_line_style );
    lv_style_set_line_width( &red_line_style, 6 );
    lv_style_set_line_color( &red_line_style, lv_palette_main( LV_PALETTE_RED ) );
    lv_style_set_line_rounded( &red_line_style, true );

    static lv_style_t green_line_style;
    lv_style_init( &green_line_style );
    lv_style_set_line_width( &green_line_style, 6 );
    lv_style_set_line_color( &green_line_style, lv_palette_main( LV_PALETTE_GREEN ) );
    lv_style_set_line_rounded( &green_line_style, true );

    static lv_style_t blue_line_style;
    lv_style_init( &blue_line_style );
    lv_style_set_line_width( &blue_line_style, 6 );
    lv_style_set_line_color( &blue_line_style, lv_palette_main( LV_PALETTE_BLUE ) );
    lv_style_set_line_rounded( &blue_line_style, true );

    /* Line container: flex row for the three button indicator lines */
    lv_obj_t *line_row = lv_obj_create( touch_tab );
    lv_obj_remove_style_all( line_row );
    lv_obj_set_width( line_row, lv_pct( 90 ) );
    lv_obj_set_height( line_row, 10 );
    lv_obj_set_layout( line_row, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( line_row, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( line_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );

    lv_obj_t *left_line = lv_line_create( line_row );
    lv_line_set_points( left_line, line_points, 2 );
    lv_obj_add_style( left_line, &red_line_style, 0 );

    lv_obj_t *middle_line = lv_line_create( line_row );
    lv_line_set_points( middle_line, line_points, 2 );
    lv_obj_add_style( middle_line, &green_line_style, 0 );
    
    lv_obj_t *right_line = lv_line_create( line_row );
    lv_line_set_points( right_line, line_points, 2 );
    lv_obj_add_style( right_line, &blue_line_style, 0 );

    lvgl_port_unlock();

    core2foraws_button_register_callback( BUTTON_LEFT, PRESS, touch_button_callback );
    core2foraws_button_register_callback( BUTTON_MIDDLE, PRESS, touch_button_callback );
    /* BUTTON_RIGHT PRESS is dispatched centrally from main.c */
}

void reset_touch_bg()
{
    r=0x00, g=0x00, b=0x00;
    lv_style_set_bg_color( &bg_style, lv_color_make( r, g, b ) );
    lv_obj_add_style( touch_bg, &bg_style, 0 );
}

void touch_on_right_press( void )
{
    ESP_LOGI( TAG, "Right button was tapped" );
    b += 0x10;

    lvgl_port_lock( 0 );
    lv_style_set_bg_color( &bg_style, lv_color_make( r, g, b ) );
    lv_obj_add_style( touch_bg, &bg_style, 0 );
    lv_label_set_text( button_touch_label, "Right button" );
    lvgl_port_unlock();
}

static void touch_button_callback( enum core2foraws_button_btns button, press_event_t event )
{
    if ( event != PRESS ) return;

    if ( button == BUTTON_LEFT )
    {
        ESP_LOGI( TAG, "Left button was tapped" );
        r += 0x10;

        lvgl_port_lock( 0 );
        lv_style_set_bg_color( &bg_style, lv_color_make( r, g, b ) );
        lv_obj_add_style( touch_bg, &bg_style, 0 );
        lv_label_set_text( button_touch_label, "Left button" );
        lvgl_port_unlock();
    }
    else if ( button == BUTTON_MIDDLE )
    {
        ESP_LOGI( TAG, "Middle button was tapped" );
        g += 0x10;

        lvgl_port_lock( 0 );
        lv_style_set_bg_color( &bg_style, lv_color_make( r, g, b ) );
        lv_obj_add_style( touch_bg, &bg_style, 0 );
        lv_label_set_text( button_touch_label, "Middle button" );
        lvgl_port_unlock();
    }
    else if ( button == BUTTON_RIGHT )
    {
        touch_on_right_press();
    }
}