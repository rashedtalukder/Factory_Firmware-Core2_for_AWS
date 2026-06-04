/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * power.c
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
#include "power.h"

static void led_event_handler( lv_event_t *e );
static void vibration_event_handler( lv_event_t *e );
static void brightness_event_handler( lv_event_t *e );

static const char *TAG = POWER_TAB_NAME;

lv_obj_t *power_tab;
TaskHandle_t power_handle;

void display_power_tab( lv_obj_t *tv, battery_labels_t *bat_labels )
{
    lvgl_port_lock( 0 );

    power_tab = ui_tabview_add_tab( tv, POWER_TAB_NAME );

    /* Card with flex-column layout */
    lv_obj_t *card = ui_create_card( power_tab, lv_color_make( 255, 97, 56 ) );
    ui_card_title( card, "AXP192 Power Mgmt", lv_color_make(0,0,0) );
    ui_card_text( card, "The AXP192 provides power management for the battery and on-board peripherals.\n\nTap to toggle:", lv_color_make(0,0,0) );

    /* Button row: LED | Motor | Screen — horizontal flex */
    lv_obj_t *btn_row = lv_obj_create( card );
    lv_obj_remove_style_all( btn_row );
    lv_obj_set_width( btn_row, lv_pct( 100 ) );
    lv_obj_set_height( btn_row, LV_SIZE_CONTENT );
    lv_obj_set_layout( btn_row, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( btn_row, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_flex_grow( btn_row, 1 );

    lv_obj_t *pwr_led_btn = lv_button_create( btn_row );
    lv_obj_set_size( pwr_led_btn, 76, 38 );
    lv_obj_add_flag( pwr_led_btn, LV_OBJ_FLAG_CHECKABLE );
    lv_obj_add_state( pwr_led_btn, LV_STATE_CHECKED );
    lv_obj_add_event_cb( pwr_led_btn, led_event_handler, LV_EVENT_VALUE_CHANGED, NULL );
    lv_obj_t *led_label = lv_label_create( pwr_led_btn );
    lv_label_set_text_static( led_label, "LED" );

    lv_obj_t *vibr_btn = lv_button_create( btn_row );
    lv_obj_set_size( vibr_btn, 76, 38 );
    lv_obj_add_flag( vibr_btn, LV_OBJ_FLAG_CHECKABLE );
    lv_obj_add_event_cb( vibr_btn, vibration_event_handler, LV_EVENT_VALUE_CHANGED, NULL );
    lv_obj_t *vibr_label = lv_label_create( vibr_btn );
    lv_label_set_text_static( vibr_label, "Motor" );

    lv_obj_t *scrn_btn = lv_button_create( btn_row );
    lv_obj_set_size( scrn_btn, 76, 38 );
    lv_obj_add_flag( scrn_btn, LV_OBJ_FLAG_CHECKABLE );
    lv_obj_add_state( scrn_btn, LV_STATE_CHECKED );
    lv_obj_add_event_cb( scrn_btn, brightness_event_handler, LV_EVENT_VALUE_CHANGED, NULL );
    lv_obj_t *brightness_label = lv_label_create( scrn_btn );
    lv_label_set_text_static( brightness_label, "Screen" );

    lvgl_port_unlock();

    xTaskCreatePinnedToCore( battery_task, "batteryTask", configMINIMAL_STACK_SIZE * 2, ( void * ) bat_labels, 0, &power_handle, 1 );
}

static void brightness_event_handler( lv_event_t *e )
{
    lv_obj_t *obj = lv_event_get_target( e );
    bool checked = lv_obj_has_state( obj, LV_STATE_CHECKED );

    if ( !checked )
        core2foraws_power_backlight_set( DISPLAY_BACKLIGHT_START / 2 );
    else
        core2foraws_power_backlight_set( DISPLAY_BACKLIGHT_START );
    
    ESP_LOGI( TAG, "Screen brightness: %d", checked );
}

static void led_event_handler( lv_event_t *e )
{
    lv_obj_t *obj = lv_event_get_target( e );
    bool checked = lv_obj_has_state( obj, LV_STATE_CHECKED );

    core2foraws_power_led_enable( checked );
    ESP_LOGI( TAG, "LED state: %d", checked );
}

static void vibration_event_handler( lv_event_t *e )
{
    lv_obj_t *obj = lv_event_get_target( e );
    bool checked = lv_obj_has_state( obj, LV_STATE_CHECKED );

    if ( !checked )
        core2foraws_power_vibration_enable( 0 );
    else
        core2foraws_power_vibration_enable( 60 );
    
    ESP_LOGI( TAG, "Vibration motor state: %d", checked );
}

void battery_task( void *pvParameters )
{
    battery_labels_t *labels = ( battery_labels_t * )pvParameters;
    lv_obj_t *battery_label = labels->battery_label;
    lv_obj_t *charge_label = labels->charge_label;

    for( ; ; )
    {
        float battery_voltage;
        core2foraws_power_batt_volts_get( &battery_voltage );

        bool charging;
        core2foraws_power_plugged_get( &charging );

        lvgl_port_lock( 0 );
        if (battery_voltage >= 4.100)
        {
            lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL);
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0x0ab300), 0);
        } 
        else if ( battery_voltage >= 3.95)
        {
            lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_3);
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0x0ab300), 0);
        }
        else if ( battery_voltage >= 3.80)
        {
            lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_2);
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0xff9900), 0);
        }
        else if ( battery_voltage >= 3.25)
        {
            lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_1);
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0xff0000), 0);
        }
        else
        {
            lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_EMPTY);
            lv_obj_set_style_text_color(battery_label, lv_color_hex(0xff0000), 0);
        }

        if ( charging )
        {
            lv_label_set_text( charge_label, LV_SYMBOL_CHARGE );
            lv_obj_set_style_text_color( charge_label, lv_color_hex(0x0000cc), 0 );
        }
        else
        {
            lv_label_set_text( charge_label, "" );
        }
        lvgl_port_unlock();
        vTaskDelay( pdMS_TO_TICKS( 200 ) );
    }
}