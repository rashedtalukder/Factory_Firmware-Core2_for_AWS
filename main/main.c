/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * main.c
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
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "sound.h"
#include "home.h"
#include "wifi.h"
#include "mpu.h"
#include "mic.h"
#include "clock.h"
#include "power.h"
#include "touch.h"
#include "led_bar.h"
#include "crypto.h"
#include "cta.h"
#include "screenshot.h"
#ifdef CONFIG_UITEST_ENABLED
#include "uitest.h"
#endif

static const char *TAG = "MAIN";

static void ui_start(void);
static void tab_event_cb(lv_event_t *e);
static void right_button_dispatch_cb( enum core2foraws_button_btns button, press_event_t event );

static void screenshot_button_cb(enum core2foraws_button_btns button,
                                  press_event_t event)
{
    if (button == BUTTON_MIDDLE && event == LONGPRESS)
        screenshot_take();
}

static lv_obj_t *tab_view;
static lv_obj_t *page_dots[10];
static lv_obj_t *page_title_label;

static battery_labels_t bat_labels;

#define NUM_TABS 10

TaskHandle_t    clock_handle,
                led_bar_animation_handle, 
                led_bar_solid_handle;

LV_IMAGE_DECLARE( powered_by_aws_logo );

void app_main( void )
{
    ESP_LOGI( TAG, "\n***************************************************\n M5Stack Core2 for AWS IoT EduKit Factory Firmware\n***************************************************" );

    esp_log_level_set( "gpio", ESP_LOG_NONE );
    esp_log_level_set( "ILI9341", ESP_LOG_NONE );

    core2foraws_init(); // Initializes the enabled hardware drivers and calls their respective initialization functions.
    ESP_LOGI( TAG, "Hardware drivers initialized" );

    ui_start(); // Starts all the sensor readings and shows them on the display using the LVGL library

#ifdef CONFIG_UITEST_ENABLED
    uitest_init();
#endif

    screenshot_init();
    core2foraws_button_register_callback(BUTTON_MIDDLE, LONGPRESS, screenshot_button_cb);

    ESP_LOGI( TAG, "Factory firmware ready" );
}

static void ui_start( void )
{
    /* Displays the Powered by AWS logo */
    lvgl_port_lock( 0 );
    lv_obj_t *opener_scr = lv_screen_active();
    lv_obj_t *aws_img_obj = lv_image_create( opener_scr );
    lv_image_set_src( aws_img_obj, &powered_by_aws_logo );
    lv_obj_align( aws_img_obj, LV_ALIGN_CENTER, 0, 0 );
    lv_obj_set_style_bg_color( opener_scr, lv_color_make(255,255,255), 0 );
    lv_obj_set_style_bg_opa( opener_scr, LV_OPA_COVER, 0 );
    lvgl_port_unlock();

    vTaskDelay( pdMS_TO_TICKS( 1500 ) );
    
    xTaskCreatePinnedToCore( sound_task, "soundTask", 4096 * 2, NULL, 4, NULL, 1 );
    
    lvgl_port_lock( 0 );
    lv_obj_clean( opener_scr );
    lv_obj_t *core2forAWS_obj = lv_obj_create( NULL );
    lv_obj_set_style_bg_color( core2forAWS_obj, lv_color_hex( UI_SCREEN_BG_COLOR ), 0 );
    lv_obj_set_style_bg_opa( core2forAWS_obj, LV_OPA_COVER, 0 );
    lv_screen_load_anim( core2forAWS_obj, LV_SCREEN_LOAD_ANIM_MOVE_LEFT, 400, 0, false );

    /* Root layout: flex column → top_bar + tabview stack vertically */
    lv_obj_set_layout( core2forAWS_obj, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( core2forAWS_obj, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_style_pad_all( core2forAWS_obj, 0, 0 );
    lv_obj_set_style_pad_row( core2forAWS_obj, 0, 0 );

    /* ── Top bar: absolute children → title left, dots center, battery right ── */
    lv_obj_t *top_bar = lv_obj_create( core2forAWS_obj );
    lv_obj_remove_style_all( top_bar );
    lv_obj_set_size( top_bar, lv_pct( 100 ), 30 );
    lv_obj_remove_flag( top_bar, LV_OBJ_FLAG_SCROLLABLE );

    /* Page title label — pinned left */
    page_title_label = lv_label_create( top_bar );
    lv_label_set_text_static( page_title_label, "Home" );
    lv_obj_set_style_text_color( page_title_label, lv_color_hex( 0xffffff ), 0 );
    lv_obj_set_style_text_font( page_title_label, LV_FONT_DEFAULT, 0 );
    lv_obj_align( page_title_label, LV_ALIGN_LEFT_MID, 12, 0 );

    /* Dot indicators — true screen center */
    lv_obj_t *dot_container = lv_obj_create( top_bar );
    lv_obj_remove_style_all( dot_container );
    lv_obj_set_size( dot_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT );
    lv_obj_set_layout( dot_container, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( dot_container, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( dot_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_column( dot_container, 6, 0 );
    lv_obj_center( dot_container );

    for ( int i = 0; i < NUM_TABS; i++ )
    {
        page_dots[i] = lv_obj_create( dot_container );
        lv_obj_remove_style_all( page_dots[i] );
        lv_obj_set_size( page_dots[i], 8, 8 );
        lv_obj_set_style_radius( page_dots[i], LV_RADIUS_CIRCLE, 0 );
        lv_obj_set_style_bg_opa( page_dots[i], LV_OPA_COVER, 0 );
        lv_obj_set_style_bg_color( page_dots[i], ( i == 0 ) ? lv_color_hex( UI_ACCENT_COLOR ) : lv_color_hex( UI_DOT_INACTIVE ), 0 );
        lv_obj_remove_flag( page_dots[i], LV_OBJ_FLAG_CLICKABLE );
    }

    /* Battery — fixed container pinned right, glyphs centered inside */
    lv_obj_t *battery_container = lv_obj_create( top_bar );
    lv_obj_remove_style_all( battery_container );
    lv_obj_set_size( battery_container, 22, 18 );
    lv_obj_align( battery_container, LV_ALIGN_RIGHT_MID, -8, 0 );

    bat_labels.battery_label = lv_label_create( battery_container );
    lv_label_set_text( bat_labels.battery_label, LV_SYMBOL_BATTERY_FULL );
    lv_obj_set_width( bat_labels.battery_label, 22 );
    lv_obj_set_style_text_align( bat_labels.battery_label, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_set_style_text_color( bat_labels.battery_label, lv_color_hex( 0x0ab300 ), 0 );
    lv_obj_center( bat_labels.battery_label );

    bat_labels.charge_label = lv_label_create( battery_container );
    lv_label_set_text( bat_labels.charge_label, "" );
    lv_obj_center( bat_labels.charge_label );

    /* ── Tabview: grows to fill remaining space ───────────────────────── */
    tab_view = lv_tabview_create( core2forAWS_obj );
    lv_tabview_set_tab_bar_position( tab_view, LV_DIR_TOP );
    lv_tabview_set_tab_bar_size( tab_view, 0 );
    lv_obj_set_width( tab_view, lv_pct( 100 ) );
    lv_obj_set_flex_grow( tab_view, 1 );
    lv_obj_add_event_cb( tab_view, tab_event_cb, LV_EVENT_VALUE_CHANGED, NULL );
    
    lvgl_port_unlock();

    /*
    Below creates all the display layers for the various peripheral tabs. Some of the tabs also starts the concurrent FreeRTOS tasks 
    that read/write to the peripheral registers and displays the data from that peripheral.
    */
    ESP_LOGD( TAG, "Building UI tabs" );
    display_home_tab( tab_view );
    display_clock_tab( tab_view );
    display_mpu_tab( tab_view );
    display_microphone_tab( tab_view );
    display_LED_bar_tab( tab_view );
    display_power_tab( tab_view, &bat_labels );
    display_touch_tab( tab_view );
    display_crypto_tab( tab_view );
    display_wifi_tab( tab_view );
    display_cta_tab( tab_view );

    /* Single BUTTON_RIGHT PRESS dispatch — registered last so it wins.
     * Routes to the right handler depending on the active tab. */
    core2foraws_button_register_callback( BUTTON_RIGHT, PRESS, right_button_dispatch_cb );

    ESP_LOGD( TAG, "UI ready" );
}

static const char *tab_names[] = {
    HOME_TAB_NAME, CLOCK_TAB_NAME, MPU_TAB_NAME, MICROPHONE_TAB_NAME,
    LED_BAR_TAB_NAME, POWER_TAB_NAME, TOUCH_TAB_NAME, CRYPTO_TAB_NAME,
    WIFI_TAB_NAME, CTA_TAB_NAME
};

/* Routes BUTTON_RIGHT PRESS to the correct tab handler */
static void right_button_dispatch_cb( enum core2foraws_button_btns button, press_event_t event )
{
    lvgl_port_lock( 0 );
    uint16_t idx = lv_tabview_get_tab_active( tab_view );
    lvgl_port_unlock();
    ESP_LOGD( TAG, "Right button pressed on tab: %s", tab_names[ idx ] );
    if ( strcmp( tab_names[ idx ], CLOCK_TAB_NAME ) == 0 )
        clock_on_right_press();
    else
        touch_on_right_press();
}

static const char *tab_display_names[] = {
    "Home", "Clock", "IMU", "Mic",
    "LEDs", "Power", "Touch", "Crypto",
    "Wi-Fi", "Next Steps"
};

static void tab_event_cb( lv_event_t *e )
{
    uint16_t tab_idx = lv_tabview_get_tab_active( tab_view );
    const char *tab_name = tab_names[ tab_idx ];
    ESP_LOGI( TAG, "Active tab: %s", tab_name );

    /* Update page indicator dots */
    for ( int i = 0; i < NUM_TABS; i++ )
        lv_obj_set_style_bg_color( page_dots[i], ( i == tab_idx ) ? lv_color_hex( UI_ACCENT_COLOR ) : lv_color_hex( UI_DOT_INACTIVE ), 0 );
    lv_label_set_text_static( page_title_label, tab_display_names[ tab_idx ] );

    /* Suspend all per-tab worker tasks. These are guarded against NULL: a task
     * handle is NULL if its xTaskCreate failed (e.g. low internal RAM). Passing
     * NULL to vTaskSuspend() would suspend THIS task (taskLVGL) while it holds
     * the LVGL lock, hanging the whole UI — so every handle must be checked. */
    if ( MPU_handle )            vTaskSuspend( MPU_handle );
    if ( mic_handle )            vTaskSuspend( mic_handle );
    if ( FFT_handle )            vTaskSuspend( FFT_handle );
    if ( wifi_handle )           vTaskSuspend( wifi_handle );
    if ( led_bar_solid_handle )  vTaskSuspend( led_bar_solid_handle );
    if ( led_bar_animation_handle ) vTaskResume( led_bar_animation_handle );

    if ( strcmp( tab_name, CLOCK_TAB_NAME ) == 0 )
        update_roller_time();
    else if ( strcmp( tab_name, MPU_TAB_NAME ) == 0 )
    {
        if ( MPU_handle ) vTaskResume( MPU_handle );
    }
    else if (strcmp( tab_name, MICROPHONE_TAB_NAME ) == 0 )
    {
        if ( mic_handle ) vTaskResume( mic_handle );
        if ( FFT_handle ) vTaskResume( FFT_handle );
    } 
    else if ( strcmp( tab_name, LED_BAR_TAB_NAME ) == 0 )
    {
        if ( led_bar_animation_handle ) vTaskSuspend( led_bar_animation_handle );
        if ( led_bar_solid_handle )     vTaskResume( led_bar_solid_handle );
    }
    else if ( strcmp( tab_name, TOUCH_TAB_NAME ) == 0 )
    {
        reset_touch_bg();
    }
    else if ( strcmp( tab_name, WIFI_TAB_NAME ) == 0 )
    {
        if ( wifi_handle ) vTaskResume( wifi_handle );
    }
}