/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v2.4.0
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
static void style_toggle_button( lv_obj_t *button );

static const char *TAG = POWER_TAB_NAME;
static lv_subject_t *battery_state;

#define IDLE_DIM_AFTER_MS   60000
#define IDLE_DIM_BRIGHTNESS 20
#define IDLE_CHECK_MS       200

/* Only touched from LVGL callbacks, which run with the LVGL lock held. */
static uint8_t backlight_level = DISPLAY_BACKLIGHT_START;
static bool backlight_dimmed;

static void idle_dim_timer_cb( lv_timer_t *timer )
{
    ( void )timer;
    bool idle = lv_display_get_inactive_time( NULL ) >= IDLE_DIM_AFTER_MS;
    if ( idle == backlight_dimmed ) return;

    esp_err_t err = core2foraws_power_backlight_set( idle ? IDLE_DIM_BRIGHTNESS : backlight_level );
    if ( err != ESP_OK )
    {
        ESP_LOGW( TAG, "Idle backlight change failed: %s", esp_err_to_name( err ) );
        return;
    }
    backlight_dimmed = idle;
}

static const struct { float min_volts; const char *symbol; uint32_t color; } battery_levels[] = {
    { 4.10f, LV_SYMBOL_BATTERY_FULL,  0x0ab300 },
    { 3.95f, LV_SYMBOL_BATTERY_3,     0x0ab300 },
    { 3.80f, LV_SYMBOL_BATTERY_2,     0xff9900 },
    { 3.25f, LV_SYMBOL_BATTERY_1,     0xff0000 },
    { 0.00f, LV_SYMBOL_BATTERY_EMPTY, 0xff0000 },
};

static void battery_level_changed(lv_observer_t *observer, lv_subject_t *subject)
{
    lv_obj_t *label = lv_observer_get_target_obj(observer);
    int level = lv_subject_get_int(subject) & 7;
    lv_label_set_text_static(label, battery_levels[level].symbol);
    lv_obj_set_style_text_color(label, lv_color_hex(battery_levels[level].color), 0);
}

static void battery_charge_changed(lv_observer_t *observer, lv_subject_t *subject)
{
    lv_obj_t *label = lv_observer_get_target_obj(observer);
    bool charging = (lv_subject_get_int(subject) & 8) != 0;
    lv_label_set_text_static(label, charging ? LV_SYMBOL_CHARGE : "");
    if (charging) lv_obj_set_style_text_color(label, lv_color_hex(0x0000cc), 0);
}

lv_obj_t *battery_indicator_create(lv_obj_t *parent)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, BATTERY_INDICATOR_WIDTH, 18);

    lv_obj_t *level = lv_label_create(container);
    lv_obj_set_width(level, BATTERY_INDICATOR_WIDTH);
    lv_obj_set_style_text_align(level, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(level);

    lv_obj_t *charge = lv_label_create(container);
    lv_obj_center(charge);

    if (battery_state == NULL) battery_state = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_add_observer_obj(battery_state, battery_level_changed, level, NULL);
    lv_subject_add_observer_obj(battery_state, battery_charge_changed, charge, NULL);
    return container;
}

lv_obj_t *power_tab;
TaskHandle_t power_handle;

void display_power_tab( lv_obj_t *tv )
{
    ESP_LOGD( TAG, "Building tab" );
    lvgl_port_lock( 0 );

    power_tab = ui_tabview_add_tab( tv, POWER_TAB_NAME );

    /* Card with flex-column layout */
    lv_obj_t *card = ui_create_card( power_tab, lv_color_make( 255, 97, 56 ) );
    ui_card_title( card, "AXP192 Power Mgmt", lv_color_make(0,0,0) );
    ui_card_text( card, "The AXP192 provides power management for the battery and on-board peripherals.\n\nTap to toggle:", lv_color_make(0,0,0) );

    /* Button row: LED | Motor | Screen — horizontal flex */
    lv_obj_t *btn_row = ui_create_row( card, LV_FLEX_ALIGN_SPACE_EVENLY, 0 );
    lv_obj_set_flex_grow( btn_row, 1 );

    lv_obj_t *pwr_led_btn = lv_button_create( btn_row );
    ui_test_id(pwr_led_btn, "power.led");
    lv_obj_set_size( pwr_led_btn, 76, 38 );
    lv_obj_set_checkable( pwr_led_btn, true );
    style_toggle_button( pwr_led_btn );
    lv_obj_add_state( pwr_led_btn, LV_STATE_CHECKED );
    lv_obj_add_event_cb( pwr_led_btn, led_event_handler, LV_EVENT_VALUE_CHANGED, NULL );
    lv_obj_t *led_label = lv_label_create( pwr_led_btn );
    lv_label_set_text_static( led_label, "LED" );

    lv_obj_t *vibr_btn = lv_button_create( btn_row );
    ui_test_id(vibr_btn, "power.motor");
    lv_obj_set_size( vibr_btn, 76, 38 );
    lv_obj_set_checkable( vibr_btn, true );
    style_toggle_button( vibr_btn );
    lv_obj_add_event_cb( vibr_btn, vibration_event_handler, LV_EVENT_VALUE_CHANGED, NULL );
    lv_obj_t *vibr_label = lv_label_create( vibr_btn );
    lv_label_set_text_static( vibr_label, "Motor" );

    lv_obj_t *scrn_btn = lv_button_create( btn_row );
    ui_test_id(scrn_btn, "power.screen");
    lv_obj_set_size( scrn_btn, 76, 38 );
    lv_obj_set_checkable( scrn_btn, true );
    style_toggle_button( scrn_btn );
    lv_obj_add_state( scrn_btn, LV_STATE_CHECKED );
    lv_obj_add_event_cb( scrn_btn, brightness_event_handler, LV_EVENT_VALUE_CHANGED, NULL );
    lv_obj_t *brightness_label = lv_label_create( scrn_btn );
    lv_label_set_text_static( brightness_label, "Screen" );

    if ( lv_timer_create( idle_dim_timer_cb, IDLE_CHECK_MS, NULL ) == NULL )
        ESP_LOGE( TAG, "Failed to create idle dimming timer" );

    lvgl_port_unlock();

    if ( xTaskCreatePinnedToCore( battery_task, "batteryTask", configMINIMAL_STACK_SIZE * 2,
                                 NULL, 0, &power_handle, 1 ) != pdPASS )
    {
        power_handle = NULL;
        ESP_LOGE( TAG, "Failed to create battery task" );
    }
}

static void style_toggle_button( lv_obj_t *button )
{
    lv_obj_set_style_bg_color( button, lv_color_hex( 0xc62828 ), LV_PART_MAIN | LV_STATE_DEFAULT );
    lv_obj_set_style_bg_color( button, lv_color_hex( 0x9b1c1c ), LV_PART_MAIN | LV_STATE_PRESSED );
    lv_obj_set_style_bg_color( button, lv_color_hex( 0x2e7d32 ), LV_PART_MAIN | LV_STATE_CHECKED );
    lv_obj_set_style_bg_color( button, lv_color_hex( 0x1b5e20 ),
                               LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED );
}

static void brightness_event_handler( lv_event_t *e )
{
    lv_obj_t *obj = lv_event_get_target( e );
    bool checked = lv_obj_has_state( obj, LV_STATE_CHECKED );

    uint8_t brightness = checked ? DISPLAY_BACKLIGHT_START : DISPLAY_BACKLIGHT_START / 2;
    esp_err_t err = core2foraws_power_backlight_set( brightness );
    if ( err != ESP_OK )
    {
        ESP_LOGE( TAG, "Failed to set screen brightness: %s", esp_err_to_name( err ) );
        if (checked) lv_obj_remove_state(obj, LV_STATE_CHECKED);
        else lv_obj_add_state(obj, LV_STATE_CHECKED);
        return;
    }
    backlight_level = brightness;
    backlight_dimmed = false;
    
    ESP_LOGI( TAG, "Screen brightness: %d", checked );
}

static void led_event_handler( lv_event_t *e )
{
    lv_obj_t *obj = lv_event_get_target( e );
    bool checked = lv_obj_has_state( obj, LV_STATE_CHECKED );

    esp_err_t err = core2foraws_power_led_enable( checked );
    if ( err != ESP_OK )
    {
        ESP_LOGE( TAG, "Failed to set power LED: %s", esp_err_to_name( err ) );
        if (checked) lv_obj_remove_state(obj, LV_STATE_CHECKED);
        else lv_obj_add_state(obj, LV_STATE_CHECKED);
        return;
    }
    ESP_LOGI( TAG, "LED state: %d", checked );
}

static void vibration_event_handler( lv_event_t *e )
{
    lv_obj_t *obj = lv_event_get_target( e );
    bool checked = lv_obj_has_state( obj, LV_STATE_CHECKED );

    esp_err_t err = core2foraws_power_vibration_enable( checked );
    if ( err != ESP_OK )
    {
        ESP_LOGE( TAG, "Failed to set vibration motor: %s", esp_err_to_name( err ) );
        if (checked) lv_obj_remove_state(obj, LV_STATE_CHECKED);
        else lv_obj_add_state(obj, LV_STATE_CHECKED);
        return;
    }
    
    ESP_LOGI( TAG, "Vibration motor state: %d", checked );
}

void battery_task( void *pvParameters )
{
    (void)pvParameters;
    int shown_level = -1;
    int shown_charging = -1;

    for( ; ; )
    {
        float battery_voltage;
        esp_err_t err = core2foraws_power_batt_volts_get( &battery_voltage );

        bool charging;
        if ( err == ESP_OK )
            err = core2foraws_power_charging_get( &charging );
        if ( err != ESP_OK )
        {
            ESP_LOGW( TAG, "Battery status read failed: %s", esp_err_to_name( err ) );
            vTaskDelay( pdMS_TO_TICKS( 1000 ) );
            continue;
        }

        int level = 0;
        while ( level < ( int )( sizeof( battery_levels ) / sizeof( battery_levels[ 0 ] ) ) - 1 &&
                battery_voltage < battery_levels[ level ].min_volts )
            level++;
        /* Unchanged state would only re-render the status bar every second. */
        if ( level == shown_level && ( int )charging == shown_charging )
        {
            vTaskDelay( pdMS_TO_TICKS( 1000 ) );
            continue;
        }

        /* Bounded wait with padding so a wedged render loop can't deadlock
         * this periodic task; skip the update if the mutex isn't free. */
        if ( !lvgl_port_lock( 1000 ) )
        {
            ESP_LOGW( TAG, "LVGL lock timeout; skipping battery update" );
            vTaskDelay( pdMS_TO_TICKS( 1000 ) );
            continue;
        }
        lv_subject_set_int(battery_state, level | (charging ? 8 : 0));
        lvgl_port_unlock();
        shown_level = level;
        shown_charging = charging;
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}