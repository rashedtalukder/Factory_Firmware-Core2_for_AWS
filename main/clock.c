/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * clock.c
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
#include <freertos/FreeRTOS.h>
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "core2foraws.h"
#include "ui_helpers.h"
#include "clock.h"

static const char *TAG = CLOCK_TAB_NAME;

lv_obj_t *clock_tab;

static lv_obj_t *hour_roller;
static lv_obj_t *minute_roller;
static lv_obj_t *time_label;
static lv_obj_t *set_confirm_label;

void clock_on_right_press( void )
{
    lvgl_port_lock( 0 );
    int hour = lv_roller_get_selected( hour_roller );
    int minute = lv_roller_get_selected( minute_roller );
    lvgl_port_unlock();

    struct tm current_time;
    core2foraws_rtc_time_get( &current_time );
    current_time.tm_hour = hour;
    current_time.tm_min = minute;
    current_time.tm_sec = 0;
    core2foraws_rtc_time_set( current_time );

    ESP_LOGI( TAG, "RTC set to %02d:%02d:00", hour, minute );

    /* Flash brief confirmation */
    lvgl_port_lock( 0 );
    lv_label_set_text_static( set_confirm_label, LV_SYMBOL_OK );
    lv_obj_set_style_text_color( set_confirm_label, lv_color_hex( 0x007700 ), 0 );
    lvgl_port_unlock();
}

void update_roller_time()
{
    /* Called from the LVGL thread on tab activation. Instead of doing a
     * blocking I2C RTC read here (which stalls rendering mid tab-transition
     * and races with clock_task's periodic read), just wake clock_task and
     * let it resync the rollers on its own thread. This keeps all RTC access
     * on a single task. */
    if ( clock_handle )
        xTaskNotifyGive( clock_handle );
}

/* Build zero-padded two-digit roller options: "00\n01\n…\n(count-1)" */
static void build_two_digit_options( char *buffer, size_t buffer_size, int count )
{
    size_t used = 0;
    for ( int i = 0; i < count; i++ )
    {
        int written = snprintf( &buffer[used], buffer_size - used,
                                ( i == ( count - 1 ) ) ? "%02d" : "%02d\n", i );
        if ( written < 0 || (size_t)written >= ( buffer_size - used ) )
            break;
        used += (size_t)written;
    }
}

void display_clock_tab( lv_obj_t *tv )
{
    lvgl_port_lock( 0 );
    clock_tab = ui_tabview_add_tab( tv, CLOCK_TAB_NAME );

    /* Card with flex-column layout */
    lv_obj_t *card = ui_create_card( clock_tab, lv_color_make( 254, 230, 0 ) );
    ui_card_title( card, "BM8563 Real-time Clock", lv_color_make(0,0,0) );

    /* Live time display inside the card */
    time_label = lv_label_create( card );
    lv_label_set_text( time_label, "00:00:00 AM" );
    lv_obj_set_style_text_color( time_label, lv_color_make(0,0,0), 0 );
    lv_obj_set_style_text_align( time_label, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_set_width( time_label, lv_pct( 100 ) );

    /* Roller row: hour : minute — horizontal flex inside the card */
    lv_obj_t *roller_row = lv_obj_create( card );
    lv_obj_remove_style_all( roller_row );
    lv_obj_set_width( roller_row, lv_pct( 100 ) );
    lv_obj_set_height( roller_row, LV_SIZE_CONTENT );
    lv_obj_set_layout( roller_row, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( roller_row, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( roller_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_column( roller_row, 8, 0 );
    lv_obj_set_flex_grow( roller_row, 1 );

    /* Zero-padded roller options (stack buffers — rollers copy them) */
    char hours_str[24 * 3];
    build_two_digit_options( hours_str, sizeof( hours_str ), 24 );
    hour_roller = lv_roller_create( roller_row );
    lv_roller_set_options( hour_roller, hours_str, LV_ROLLER_MODE_NORMAL );
    lv_roller_set_visible_row_count( hour_roller, 2 );
    lv_obj_set_width( hour_roller, 60 );

    lv_obj_t *separator_label = lv_label_create( roller_row );
    lv_label_set_text_static( separator_label, ":" );

    char minutes_str[60 * 3];
    build_two_digit_options( minutes_str, sizeof( minutes_str ), 60 );
    minute_roller = lv_roller_create( roller_row );
    lv_roller_set_options( minute_roller, minutes_str, LV_ROLLER_MODE_NORMAL );
    lv_roller_set_visible_row_count( minute_roller, 2 );
    lv_obj_set_width( minute_roller, 60 );

    /* "Set" hint — floating so flex layout doesn't claim it; pinned bottom-right */
    set_confirm_label = lv_label_create( clock_tab );
    lv_label_set_text_static( set_confirm_label, "Set" );
    lv_obj_set_style_text_color( set_confirm_label, lv_color_hex( UI_ACCENT_COLOR ), 0 );
    lv_obj_add_flag( set_confirm_label, LV_OBJ_FLAG_FLOATING );
    lv_obj_align( set_confirm_label, LV_ALIGN_BOTTOM_RIGHT, -40, -4 );

    lvgl_port_unlock();

    xTaskCreatePinnedToCore( clock_task, "clockTask", configMINIMAL_STACK_SIZE * 3, NULL, 0, &clock_handle, 1 );
}

void clock_task( void *pvParameters )
{
    for( ; ; )
    {
        /* Wake once per second to refresh the live time, or immediately when
         * the clock tab is (re)opened (update_roller_time() notifies us). */
        uint32_t refresh_rollers = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS( 1000 ) );

        struct tm current_time;
        core2foraws_rtc_time_get( &current_time );
        char clock_buf[ 26 ];
        strftime( clock_buf, 26, "%I:%M:%S %p", &current_time );

        lvgl_port_lock( 0 );
        lv_label_set_text( time_label, clock_buf );
        if( refresh_rollers )
        {
            lv_roller_set_selected( hour_roller, current_time.tm_hour, LV_ANIM_OFF );
            lv_roller_set_selected( minute_roller, current_time.tm_min, LV_ANIM_OFF );
            lv_label_set_text_static( set_confirm_label, "Set" );
            lv_obj_set_style_text_color( set_confirm_label, lv_color_hex( UI_ACCENT_COLOR ), 0 );
        }
        lvgl_port_unlock();
    }
}