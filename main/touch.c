/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * touch.c
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

/**
 * @file touch.c
 * @brief FT6336U capacitive touch tab UI and virtual front-panel button handlers.
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
static bool bg_style_ready;
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
    if ( !bg_style_ready )
    {
        lv_style_init( &bg_style );
        bg_style_ready = true;
    }
    lv_style_set_bg_color( &bg_style, lv_color_make( r, g, b ) );
    lv_obj_add_style( touch_bg, &bg_style, 0 );

    ui_card_title( touch_bg, "FT6336U Capacitive Touch", lv_color_make(255,255,255) );
    ui_card_text( touch_bg,
                  "The FT6336U reports the X and Y coordinates of each touch.",
                  lv_color_make(255,255,255) );

    lv_obj_t *status_row = ui_card_action( touch_bg, "Press the touch buttons below:", lv_color_make(255,255,255), LV_FLEX_ALIGN_CENTER );
    button_touch_label = ui_value_label( status_row, "No button pressed yet" );
    ui_test_id(button_touch_label, "touch.status");

    ui_button_indicators_create( touch_tab, TOUCH_LEFT_COLOR, TOUCH_MIDDLE_COLOR, TOUCH_RIGHT_COLOR );

    lvgl_port_unlock();

    esp_err_t err = core2foraws_button_register_callback( BUTTON_LEFT, PRESS, touch_button_callback );
    if ( err != ESP_OK )
        ESP_LOGE( TAG, "Failed to register left button: %s", esp_err_to_name( err ) );

    err = core2foraws_button_register_callback( BUTTON_MIDDLE, PRESS, touch_button_callback );
    if ( err != ESP_OK )
        ESP_LOGE( TAG, "Failed to register middle button: %s", esp_err_to_name( err ) );
    /* BUTTON_RIGHT PRESS is dispatched centrally from main.c */
}

/* Call from the LVGL task (or with the LVGL lock held). */
void touch_set_active( bool active )
{
    if ( !active )
    {
        atomic_store( &touch_tab_active, false );
        return;
    }
    r = 0;
    g = 0;
    b = 0;
    update_touch_card( "No button pressed yet", 0xffffff );
    atomic_store( &touch_tab_active, true );
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

    if ( !lvgl_port_lock( 1000 ) ) return;
    if (atomic_load(&touch_tab_active)) {
        b += 0x10;
        update_touch_card( "Right button", TOUCH_RIGHT_COLOR );
    }
    lvgl_port_unlock();
}

static void touch_button_callback( enum core2foraws_button_btns button, press_event_t event )
{
    if ( event != PRESS || !atomic_load( &touch_tab_active ) ) return;

    if ( button == BUTTON_LEFT )
    {
        ESP_LOGI( TAG, "Left button was tapped" );

        if ( !lvgl_port_lock( 1000 ) ) return;
        if (atomic_load(&touch_tab_active)) {
            r += 0x10;
            update_touch_card( "Left button", TOUCH_LEFT_COLOR );
        }
        lvgl_port_unlock();
    }
    else if ( button == BUTTON_MIDDLE )
    {
        ESP_LOGI( TAG, "Middle button was tapped" );

        if ( !lvgl_port_lock( 1000 ) ) return;
        if (atomic_load(&touch_tab_active)) {
            g += 0x10;
            update_touch_card( "Middle button", TOUCH_MIDDLE_COLOR );
        }
        lvgl_port_unlock();
    }
    else if ( button == BUTTON_RIGHT )
    {
        touch_on_right_press();
    }
}