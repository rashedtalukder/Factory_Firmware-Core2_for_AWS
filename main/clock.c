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
#include <stdatomic.h>
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
static atomic_uint pending_time;

void clock_on_right_press( void )
{
    lvgl_port_lock( 0 );
    int hour = lv_roller_get_selected( hour_roller );
    int minute = lv_roller_get_selected( minute_roller );
    lvgl_port_unlock();
    atomic_store(&pending_time, (unsigned int)(hour * 60 + minute + 1));
    if (clock_handle) xTaskNotifyGive(clock_handle);
}

static void set_time_cb(lv_event_t *event)
{
    (void)event;
    clock_on_right_press();
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
    ESP_LOGD( TAG, "Building tab" );
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
    ui_test_id(hour_roller, "clock.hour");
    lv_roller_set_options( hour_roller, hours_str, LV_ROLLER_MODE_NORMAL );
    lv_roller_set_visible_row_count( hour_roller, 2 );
    lv_obj_set_width( hour_roller, 60 );

    lv_obj_t *separator_label = lv_label_create( roller_row );
    lv_label_set_text_static( separator_label, ":" );

    char minutes_str[60 * 3];
    build_two_digit_options( minutes_str, sizeof( minutes_str ), 60 );
    minute_roller = lv_roller_create( roller_row );
    ui_test_id(minute_roller, "clock.minute");
    lv_roller_set_options( minute_roller, minutes_str, LV_ROLLER_MODE_NORMAL );
    lv_roller_set_visible_row_count( minute_roller, 2 );
    lv_obj_set_width( minute_roller, 60 );

    lv_obj_t *set_button = lv_button_create(roller_row);
    lv_obj_set_size(set_button, 54, 30);
    lv_obj_add_event_cb(set_button, set_time_cb, LV_EVENT_CLICKED, NULL);
    ui_test_id(set_button, "clock.set");
    set_confirm_label = lv_label_create( set_button );
    lv_label_set_text_static( set_confirm_label, "Set" );
    lv_obj_center(set_confirm_label);
    ui_test_id(set_confirm_label, "clock.result");

    lvgl_port_unlock();

    if ( xTaskCreatePinnedToCore( clock_task, "clockTask", configMINIMAL_STACK_SIZE * 3,
                                 NULL, 0, &clock_handle, 1 ) != pdPASS )
    {
        clock_handle = NULL;
        ESP_LOGE( TAG, "Failed to create clock task" );
    }
}

void clock_task( void *pvParameters )
{    for( ; ; )
    {
        /* Wake once per second to refresh the live time, or immediately when
         * the clock tab is (re)opened (update_roller_time() notifies us). */
        uint32_t refresh_rollers = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS( 1000 ) );
        unsigned int requested = atomic_exchange(&pending_time, 0);

        struct tm current_time;
        esp_err_t err = core2foraws_rtc_time_get( &current_time );
        if (err == ESP_OK && requested != 0) {
            current_time.tm_hour = (requested - 1) / 60;
            current_time.tm_min = (requested - 1) % 60;
            current_time.tm_sec = 0;
            err = core2foraws_rtc_time_set(current_time);
        }
        if ( err != ESP_OK )
        {
            ESP_LOGW( TAG, "RTC read failed: %s", esp_err_to_name( err ) );
            if (requested != 0 && lvgl_port_lock(1000)) {
                lv_label_set_text(set_confirm_label, "Error");
                lvgl_port_unlock();
            }
            continue;
        }
        char clock_buf[ 26 ];
        strftime( clock_buf, 26, "%I:%M:%S %p", &current_time );

        /* Bounded wait with padding: a healthy LVGL loop frees the mutex
         * within a few ms. If the render loop is wedged, skip this refresh
         * instead of blocking forever (which would deadlock this task too). */
        if ( lvgl_port_lock( 1000 ) )
        {
            lv_label_set_text( time_label, clock_buf );
            if( refresh_rollers )
            {
                lv_roller_set_selected( hour_roller, current_time.tm_hour, LV_ANIM_OFF );
                lv_roller_set_selected( minute_roller, current_time.tm_min, LV_ANIM_OFF );
                lv_label_set_text_static( set_confirm_label, requested != 0 ? "Saved" : "Set" );
            }
            lvgl_port_unlock();
        }
        else
        {
            ESP_LOGW( TAG, "LVGL lock timeout; skipping clock refresh" );
        }
    }
}