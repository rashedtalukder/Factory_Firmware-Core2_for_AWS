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
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "touch.h"

static const char *TAG = TOUCH_TAB_NAME;

#define TOUCH_LEFT_COLOR   0xff5a5f
#define TOUCH_MIDDLE_COLOR 0x66ff66
#define TOUCH_RIGHT_COLOR  0x66b3ff

static uint8_t r, g, b;
static lv_style_t bg_style;
static lv_obj_t *touch_bg;
static lv_obj_t *button_touch_label;
static atomic_bool touch_tab_active;

static void touch_button_callback( enum core2foraws_button_btns button, press_event_t event );
static void update_touch_card( const char *status, uint32_t status_color );

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
    ui_card_text( touch_bg,
                  "The FT6336U capacitive touch controller provides X and Y coordinates for touch input.",
                  lv_color_make(255,255,255) );

    lv_obj_t *instruction_label = lv_label_create( touch_bg );
    lv_label_set_text_static( instruction_label, "Press the touch buttons below" );
    lv_obj_set_style_text_color( instruction_label, lv_color_make(255,255,255), 0 );
    lv_obj_set_style_margin_top( instruction_label, 2, 0 );

    button_touch_label = lv_label_create( touch_bg );
    lv_label_set_text_static( button_touch_label, "No button pressed yet" );
    lv_obj_set_style_text_align( button_touch_label, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_set_style_margin_top( button_touch_label, 6, 0 );
    lv_obj_set_style_text_color( button_touch_label, lv_color_make(255,255,255), 0 );
    lv_obj_set_style_bg_color( button_touch_label, lv_color_make(0,0,0), 0 );
    lv_obj_set_style_bg_opa( button_touch_label, LV_OPA_70, 0 );
    lv_obj_set_style_radius( button_touch_label, 4, 0 );
    lv_obj_set_style_pad_hor( button_touch_label, 8, 0 );
    lv_obj_set_style_pad_ver( button_touch_label, 3, 0 );
    lv_obj_set_width( button_touch_label, 180 );

    /* Touch button indicator lines — flex row at bottom of tab */
    static lv_point_precise_t line_points[] = { {20, 0}, {70, 0} };

    static lv_style_t red_line_style;
    lv_style_init( &red_line_style );
    lv_style_set_line_width( &red_line_style, 6 );
    lv_style_set_line_color( &red_line_style, lv_color_hex( TOUCH_LEFT_COLOR ) );
    lv_style_set_line_rounded( &red_line_style, true );

    static lv_style_t green_line_style;
    lv_style_init( &green_line_style );
    lv_style_set_line_width( &green_line_style, 6 );
    lv_style_set_line_color( &green_line_style, lv_color_hex( TOUCH_MIDDLE_COLOR ) );
    lv_style_set_line_rounded( &green_line_style, true );

    static lv_style_t blue_line_style;
    lv_style_init( &blue_line_style );
    lv_style_set_line_width( &blue_line_style, 6 );
    lv_style_set_line_color( &blue_line_style, lv_color_hex( TOUCH_RIGHT_COLOR ) );
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

    esp_err_t err = core2foraws_button_register_callback( BUTTON_LEFT, PRESS, touch_button_callback );
    if ( err != ESP_OK )
        ESP_LOGE( TAG, "Failed to register left button: %s", esp_err_to_name( err ) );

    err = core2foraws_button_register_callback( BUTTON_MIDDLE, PRESS, touch_button_callback );
    if ( err != ESP_OK )
        ESP_LOGE( TAG, "Failed to register middle button: %s", esp_err_to_name( err ) );
    /* BUTTON_RIGHT PRESS is dispatched centrally from main.c */
}

void touch_set_active( bool active )
{
    atomic_store( &touch_tab_active, active );
    if ( active )
    {
        r = 0;
        g = 0;
        b = 0;
        update_touch_card( "No button pressed yet", 0xffffff );
    }
}

static void update_touch_card( const char *status, uint32_t status_color )
{
    lv_style_set_bg_color( &bg_style, lv_color_make( r, g, b ) );
    lv_obj_report_style_change( &bg_style );
    lv_label_set_text( button_touch_label, status );
    lv_obj_set_style_text_color( button_touch_label, lv_color_hex( status_color ), 0 );
}

void touch_on_right_press( void )
{
    if ( !atomic_load( &touch_tab_active ) ) return;

    ESP_LOGI( TAG, "Right button was tapped" );
    b += 0x10;

    lvgl_port_lock( 0 );
    update_touch_card( "Right button", TOUCH_RIGHT_COLOR );
    lvgl_port_unlock();
}

static void touch_button_callback( enum core2foraws_button_btns button, press_event_t event )
{
    if ( event != PRESS || !atomic_load( &touch_tab_active ) ) return;

    if ( button == BUTTON_LEFT )
    {
        ESP_LOGI( TAG, "Left button was tapped" );
        r += 0x10;

        lvgl_port_lock( 0 );
        update_touch_card( "Left button", TOUCH_LEFT_COLOR );
        lvgl_port_unlock();
    }
    else if ( button == BUTTON_MIDDLE )
    {
        ESP_LOGI( TAG, "Middle button was tapped" );
        g += 0x10;

        lvgl_port_lock( 0 );
        update_touch_card( "Middle button", TOUCH_MIDDLE_COLOR );
        lvgl_port_unlock();
    }
    else if ( button == BUTTON_RIGHT )
    {
        touch_on_right_press();
    }
}