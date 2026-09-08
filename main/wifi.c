/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * wifi.c
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
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "wifi.h"

#define DEFAULT_SCAN_LIST_SIZE 6
#define WIFI_STA_STARTED_BIT BIT0

TaskHandle_t wifi_handle;
static atomic_bool scan_active;

void wifi_set_active(bool active)
{
    atomic_store(&scan_active, active);
    if (wifi_handle) xTaskNotifyGive(wifi_handle);
}


static lv_obj_t *mbox;
static lv_obj_t *scan_status;
static lv_style_t modal_style;

static const char *TAG = "WIFI_SCAN";

static void wifi_scan_task( void *pvParameters );
static void mbox_event_cb( lv_event_t *e );
static void event_handler( lv_event_t *e );

void display_wifi_tab( lv_obj_t *tv )
{
    ESP_LOGD( TAG, "Building tab" );
    lvgl_port_lock( 0 );

    lv_obj_t *wifi_tab = ui_tabview_add_tab( tv, WIFI_TAB_NAME );

    /* Card with flex-column layout */
    lv_obj_t *card = ui_create_card( wifi_tab, lv_color_make( 0, 82, 118 ) );
    ui_card_title( card, "Wi-Fi Scan (2.4GHz)", lv_color_make(255,255,255) );
    scan_status = ui_card_text(card, "Ready", lv_color_make(255,255,255));
    ui_test_id(scan_status, "wifi.state");

    /* AP list fills remaining card space */
    lv_obj_t *ap_list = lv_list_create( card );
    ui_test_id(ap_list, "wifi.results");
    lv_obj_set_width( ap_list, lv_pct( 100 ) );
    lv_obj_set_flex_grow( ap_list, 1 );

    /* Set the background for the popup modal */
    lv_style_init( &modal_style );
    lv_style_set_bg_color( &modal_style, lv_color_make(0,0,0) );

    lvgl_port_unlock();
    if ( xTaskCreatePinnedToCore( wifi_scan_task, "WiFiScanTask", configMINIMAL_STACK_SIZE * 4,
                                 (void*)ap_list, 1, &wifi_handle, 1 ) != pdPASS )
    {
        wifi_handle = NULL;
        ESP_LOGE( TAG, "Failed to create WiFiScanTask (low internal memory)" );
    }
}

static void opa_anim( void *bg, int32_t v )
{
    lv_obj_set_style_bg_opa( bg, v, 0 );
}

static void mbox_event_cb( lv_event_t *e )
{
    lv_event_code_t code = lv_event_get_code( e );
    lv_obj_t *obj = lv_event_get_target( e );
    if ( code == LV_EVENT_DELETE && obj == mbox )
    {
        /* Delete the parent modal background */
        lv_obj_delete_async( lv_obj_get_parent( mbox ) );
        mbox = NULL;
    }
    else if ( code == LV_EVENT_CLICKED )
    {
        /* Button was clicked */
        lv_msgbox_close( mbox );
    }
}

static void event_handler( lv_event_t *e )
{
    lv_event_code_t code = lv_event_get_code( e );
    lv_obj_t *btn = lv_event_get_target( e );
    if ( code == LV_EVENT_CLICKED )
    {
        lv_obj_t *list = lv_obj_get_parent( btn );
        ESP_LOGI( TAG, "AP selected: %s", lv_list_get_button_text( list, btn ) );
        lv_obj_t *modal_bg = lv_obj_create( lv_screen_active() );
        lv_obj_remove_style_all( modal_bg );
        lv_obj_add_style( modal_bg, &modal_style, 0 );
        lv_obj_set_pos( modal_bg, 0, 0 );
        lv_obj_set_size( modal_bg, lv_display_get_horizontal_resolution(NULL), lv_display_get_vertical_resolution(NULL) );

        /* Create the message box as a child of the modal background */
        mbox = lv_msgbox_create( modal_bg );
        lv_msgbox_add_title( mbox, "Info" );
        lv_msgbox_add_text( mbox, "Visit https://edukit.workshop.aws\n first to start building IoT apps" );
        lv_obj_t *ok_btn = lv_msgbox_add_footer_button( mbox, "Ok" );
        lv_obj_add_event_cb( ok_btn, mbox_event_cb, LV_EVENT_CLICKED, NULL );
        lv_obj_add_event_cb( mbox, mbox_event_cb, LV_EVENT_DELETE, NULL );
        lv_obj_align( mbox, LV_ALIGN_CENTER, 0, 0 );

        /* Fade the message box in with an animation */
        lv_anim_t a;
        lv_anim_init( &a );
        lv_anim_set_var( &a, modal_bg );
        lv_anim_set_time( &a, 500 );
        lv_anim_set_values( &a, LV_OPA_TRANSP, LV_OPA_50 );
        lv_anim_set_exec_cb( &a, ( lv_anim_exec_xcb_t )opa_anim );
        lv_anim_start( &a );
    }
}

static void wifi_scan_task( void *pvParameters )
{
    while (!atomic_load(&scan_active)) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    core2foraws_common_heap_report( TAG, NULL );

    /*Add buttons to the list*/
    lv_obj_t *list_btn;

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[ DEFAULT_SCAN_LIST_SIZE ];
    memset( ap_info, 0, sizeof( ap_info ) );

    while( 1 )
    {
        if (!atomic_load(&scan_active)) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
            continue;
        }
        number = DEFAULT_SCAN_LIST_SIZE;
        if (lvgl_port_lock(1000)) {
            lv_label_set_text(scan_status, "Scanning...");
            lvgl_port_unlock();
        }
        esp_err_t scan_err = core2foraws_wifi_scan(ap_info, &number);
        if ( scan_err != ESP_OK )
        {
            ESP_LOGE( TAG, "Wi-Fi scan start failed: 0x%x", scan_err );
            if (lvgl_port_lock(1000)) {
                lv_label_set_text_fmt(scan_status, "Scan failed: %s", esp_err_to_name(scan_err));
                lvgl_port_unlock();
            }
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000));
            continue;
        }
        if (!atomic_load(&scan_active) || !lvgl_port_lock(1000)) continue;
        lv_label_set_text(scan_status, "Scan complete");
        lv_obj_clean((lv_obj_t *)pvParameters);

        if ( number == 0 )
        {
            lv_list_add_text( ( lv_obj_t * )pvParameters, "No networks found" );
        }
        
        for ( int i = 0; i < number; i++ )
        {
                const char *ssid = ap_info[ i ].ssid[ 0 ] ? ( char * )ap_info[ i ].ssid : "<hidden network>";
                list_btn = lv_list_add_button( ( lv_obj_t * )pvParameters, LV_SYMBOL_WIFI, ssid );
                lv_obj_add_event_cb( list_btn, event_handler, LV_EVENT_CLICKED, NULL );

            ESP_LOGI( TAG, "SSID \t\t%s", ap_info[ i ].ssid );
            ESP_LOGI( TAG, "RSSI \t\t%d", ap_info[ i ].rssi );
            ESP_LOGI( TAG, "Channel \t\t%d\n", ap_info[ i ].primary );
        }
        lvgl_port_unlock();
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
    }
}