/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * wifi.c
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_wifi.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "wifi.h"

#define WIFI_SCAN_RECORDS        20
#define WIFI_RSSI_BAND_DB        6
/* The UI test registry is small, so only the first rows get IDs */
#define WIFI_TEST_ID_ROWS        4
#define WIFI_CONNECT_TIMEOUT_MS  20000
#define WIFI_SCAN_INTERVAL_MS    5000
#define WIFI_ICON_BLINK_MS       500
#define WIFI_ICON_BLUE           0x3aa0ff
#define WIFI_ICON_GRAY           0x6b6b7b
#define WIFI_GET_STARTED_URL     "https://aws-iot-kit-docs.m5stack.com"

TaskHandle_t wifi_handle;

static const char *TAG = "WIFI";

static atomic_bool scan_active;
static atomic_bool connect_requested;
static atomic_bool connect_busy;
/* Written in LVGL context before connect_requested is set; read by the Wi-Fi task. */
static char connect_ssid[ MAX_SSID_LEN + 1 ];
static char connect_password[ MAX_PASSPHRASE_LEN + 1 ];
static bool connect_open_network;
/* Owned by the Wi-Fi task */
static wifi_ap_record_t ap_records[ WIFI_SCAN_RECORDS ];
/* Rows currently shown, under the LVGL lock; -1 until the first list is built */
static int shown_count = -1;
static char shown_ssids[ WIFI_SCAN_RECORDS ][ MAX_SSID_LEN + 1 ];
static char shown_connected[ MAX_SSID_LEN + 1 ];

static lv_obj_t *scan_status;
static lv_obj_t *ap_list;
static lv_obj_t *entry_screen;
static lv_obj_t *password_area;
static lv_obj_t *dialog;
static lv_obj_t *dialog_backdrop;
static lv_style_t item_style;
static lv_style_t item_pressed_style;

static void wifi_task( void *pvParameters );
static void show_password_entry( void );

void wifi_set_active( bool active )
{
    atomic_store( &scan_active, active );
    if ( wifi_handle ) xTaskNotifyGive( wifi_handle );
}

/* ── Header status icon ─────────────────────────────────────────────── */

static core2foraws_wifi_state_t link_state( void )
{
    core2foraws_wifi_state_t state;
    /* Cannot fail with a valid pointer */
    ( void )core2foraws_wifi_state_get( &state );
    return state;
}

static void wifi_icon_timer_cb( lv_timer_t *timer )
{
    static bool blink_blue;
    lv_obj_t *icon = lv_timer_get_user_data( timer );
    core2foraws_wifi_state_t state = link_state();

    bool blue = state == CORE2FORAWS_WIFI_STATE_CONNECTED;
    if ( state == CORE2FORAWS_WIFI_STATE_CONNECTING )
    {
        blink_blue = !blink_blue;
        blue = blink_blue;
    }
    lv_obj_set_style_text_color( icon, lv_color_hex( blue ? WIFI_ICON_BLUE : WIFI_ICON_GRAY ), 0 );
}

lv_obj_t *wifi_status_icon_create( lv_obj_t *parent )
{
    lv_obj_t *icon = lv_label_create( parent );
    lv_label_set_text_static( icon, LV_SYMBOL_WIFI );
    lv_obj_set_style_text_color( icon, lv_color_hex( WIFI_ICON_GRAY ), 0 );
    lv_timer_create( wifi_icon_timer_cb, WIFI_ICON_BLINK_MS, icon );
    return icon;
}

/* ── Connect request, password entry and failure dialog ─────────────── */

static void connect_request( const char *password )
{
    snprintf( connect_password, sizeof( connect_password ), "%s", password );
    atomic_store( &connect_busy, true );
    atomic_store( &connect_requested, true );
    lv_label_set_text_fmt( scan_status, "Connecting to %s...", connect_ssid );
    if ( wifi_handle ) xTaskNotifyGive( wifi_handle );
}

static void entry_close( void )
{
    ui_overlay_close( &entry_screen );
    password_area = NULL;
}

static void entry_cancel_cb( lv_event_t *e )
{
    ( void )e;
    entry_close();
}

static void entry_ok_cb( lv_event_t *e )
{
    ( void )e;
    /* The keyboard's own OK key and the OK button can both fire */
    if ( password_area == NULL ) return;
    connect_request( lv_textarea_get_text( password_area ) );
    entry_close();
}

static lv_obj_t *entry_button_create( lv_obj_t *parent, const char *text, uint32_t color, lv_event_cb_t cb )
{
    lv_obj_t *button = lv_button_create( parent );
    lv_obj_set_style_bg_color( button, lv_color_hex( color ), 0 );
    lv_obj_add_event_cb( button, cb, LV_EVENT_CLICKED, NULL );
    lv_label_set_text_static( lv_label_create( button ), text );
    return button;
}

static void show_password_entry( void )
{
    if ( entry_screen != NULL ) return;

    ui_overlay_create( &entry_screen, lv_color_hex( UI_SCREEN_BG_COLOR ), LV_OPA_COVER );
    lv_obj_set_style_pad_all( entry_screen, 4, 0 );
    lv_obj_set_style_pad_row( entry_screen, 4, 0 );
    lv_obj_set_flex_flow( entry_screen, LV_FLEX_FLOW_COLUMN );

    lv_obj_t *bar = lv_obj_create( entry_screen );
    lv_obj_remove_style_all( bar );
    lv_obj_set_size( bar, lv_pct( 100 ), LV_SIZE_CONTENT );
    lv_obj_set_flex_flow( bar, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_column( bar, 6, 0 );

    ui_test_id( entry_button_create( bar, "Cancel", UI_DOT_INACTIVE, entry_cancel_cb ), "wifi.cancel" );

    lv_obj_t *title = lv_label_create( bar );
    lv_label_set_text( title, connect_ssid );
    lv_label_set_long_mode( title, LV_LABEL_LONG_MODE_DOTS );
    lv_obj_set_flex_grow( title, 1 );
    lv_obj_set_style_text_align( title, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_set_style_text_color( title, lv_color_white(), 0 );

    ui_test_id( entry_button_create( bar, "OK", UI_ACCENT_COLOR, entry_ok_cb ), "wifi.ok" );

    password_area = lv_textarea_create( entry_screen );
    ui_test_id( password_area, "wifi.password" );
    lv_obj_set_width( password_area, lv_pct( 100 ) );
    lv_textarea_set_one_line( password_area, true );
    lv_textarea_set_password_mode( password_area, true );
    lv_textarea_set_max_length( password_area, MAX_PASSPHRASE_LEN );
    lv_textarea_set_placeholder_text( password_area, "Password" );

    lv_obj_t *keyboard = lv_keyboard_create( entry_screen );
    lv_obj_set_width( keyboard, lv_pct( 100 ) );
    lv_obj_set_flex_grow( keyboard, 1 );
    lv_keyboard_set_textarea( keyboard, password_area );
    lv_obj_add_event_cb( keyboard, entry_ok_cb, LV_EVENT_READY, NULL );
    lv_obj_add_event_cb( keyboard, entry_cancel_cb, LV_EVENT_CANCEL, NULL );
}

static void dialog_close( void )
{
    ui_overlay_close( &dialog_backdrop );
    dialog = NULL;
}

static void dialog_open( const char *title, const char *text )
{
    dialog = ui_dialog_create( &dialog_backdrop, title, text );
    ui_test_id( dialog, "wifi.dialog" );
}

static void dialog_dismiss_cb( lv_event_t *e )
{
    ( void )e;
    dialog_close();
}

static void show_get_started( void )
{
    dialog_open( "Next steps", "Get started at " WIFI_GET_STARTED_URL );
    ui_dialog_add_button( dialog, "OK", "wifi.dialog.ok", dialog_dismiss_cb );
}

static void dialog_cancel_cb( lv_event_t *e )
{
    ( void )e;
    dialog_close();
    show_get_started();
}

static void dialog_retry_cb( lv_event_t *e )
{
    ( void )e;
    dialog_close();
    if ( connect_open_network ) connect_request( "" );
    else show_password_entry();
}

static void show_connect_failed( esp_err_t err )
{
    char text[ MAX_SSID_LEN + 64 ];
    const char *title;
    if ( err == ESP_ERR_WIFI_PASSWORD )
    {
        title = "Incorrect password";
        snprintf( text, sizeof( text ), "The password for \"%s\" was incorrect. Retry or cancel.", connect_ssid );
    }
    else if ( err == ESP_ERR_NOT_FOUND )
    {
        title = "Network not found";
        snprintf( text, sizeof( text ), "\"%s\" is out of range. Retry or cancel.", connect_ssid );
    }
    else
    {
        title = "Couldn't connect";
        snprintf( text, sizeof( text ), "Couldn't connect to \"%s\". Retry or cancel.", connect_ssid );
    }

    dialog_open( title, text );
    ui_dialog_add_button( dialog, "Cancel", "wifi.dialog.cancel", dialog_cancel_cb );
    ui_dialog_add_button( dialog, "Retry", "wifi.dialog.retry", dialog_retry_cb );
}

static void network_clicked_cb( lv_event_t *e )
{
    if ( atomic_load( &connect_busy ) || entry_screen != NULL || dialog != NULL ) return;

    lv_obj_t *row = lv_event_get_current_target_obj( e );
    snprintf( connect_ssid, sizeof( connect_ssid ), "%s", lv_label_get_text( lv_obj_get_child( row, 1 ) ) );
    connect_open_network = ( uintptr_t )lv_event_get_user_data( e ) == WIFI_AUTH_OPEN;
    if ( connect_open_network ) connect_request( "" );
    else show_password_entry();
}

/* ── Network list ───────────────────────────────────────────────────── */

/* Signal bands, then name, so small RSSI changes don't reorder rows under a finger */
static bool wifi_record_before( const wifi_ap_record_t *a, const wifi_ap_record_t *b )
{
    int band_a = ( a->rssi + 128 ) / WIFI_RSSI_BAND_DB;
    int band_b = ( b->rssi + 128 ) / WIFI_RSSI_BAND_DB;
    if ( band_a != band_b ) return band_a > band_b;
    return strcmp( ( const char * )a->ssid, ( const char * )b->ssid ) < 0;
}

/* Keeps the strongest record per SSID, drops hidden networks, and sorts strongest first. */
static uint16_t wifi_unique_networks( wifi_ap_record_t *records, uint16_t count )
{
    uint16_t unique = 0;
    for ( uint16_t i = 0; i < count; i++ )
    {
        if ( records[ i ].ssid[ 0 ] == '\0' ) continue;
        uint16_t match = 0;
        while ( match < unique &&
                strcmp( ( const char * )records[ match ].ssid, ( const char * )records[ i ].ssid ) != 0 )
        {
            match++;
        }
        if ( match == unique ) records[ unique++ ] = records[ i ];
        else if ( records[ i ].rssi > records[ match ].rssi ) records[ match ] = records[ i ];
    }

    for ( uint16_t i = 1; i < unique; i++ )
    {
        wifi_ap_record_t record = records[ i ];
        uint16_t j = i;
        while ( j > 0 && wifi_record_before( &record, &records[ j - 1 ] ) )
        {
            records[ j ] = records[ j - 1 ];
            j--;
        }
        records[ j ] = record;
    }
    return unique;
}

static void add_ap_item( const wifi_ap_record_t *ap, uint16_t index, bool connected )
{
    lv_obj_t *item = lv_button_create( ap_list );
    lv_obj_remove_style_all( item );
    lv_obj_add_style( item, &item_style, 0 );
    lv_obj_add_style( item, &item_pressed_style, LV_STATE_PRESSED );
    lv_obj_set_size( item, lv_pct( 100 ), LV_SIZE_CONTENT );
    lv_obj_set_flex_flow( item, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_add_event_cb( item, network_clicked_cb, LV_EVENT_CLICKED, ( void * )( uintptr_t )ap->authmode );
    if ( index < WIFI_TEST_ID_ROWS )
    {
        char id[ 24 ];
        snprintf( id, sizeof( id ), "wifi.network.%u", ( unsigned int )index );
        ui_test_id( item, id );
    }

    lv_label_set_text_static( lv_label_create( item ), LV_SYMBOL_WIFI );

    lv_obj_t *ssid = lv_label_create( item );
    lv_label_set_text( ssid, ( const char * )ap->ssid );
    lv_label_set_long_mode( ssid, LV_LABEL_LONG_MODE_DOTS );
    /* Dots only truncate at a fixed height; content height would wrap instead */
    lv_obj_set_height( ssid, lv_font_get_line_height( LV_FONT_DEFAULT ) );
    lv_obj_set_flex_grow( ssid, 1 );

    if ( connected )
    {
        lv_obj_t *mark = lv_label_create( item );
        lv_label_set_text_static( mark, LV_SYMBOL_OK );
        lv_obj_set_style_text_color( mark, lv_color_hex( WIFI_ICON_BLUE ), 0 );
    }

    lv_obj_t *rssi = lv_label_create( item );
    lv_label_set_text_fmt( rssi, "%d dBm", ap->rssi );
    lv_obj_set_style_text_color( rssi, lv_color_hex( 0x666666 ), 0 );
}

static bool rows_unchanged( uint16_t count, const char *connected )
{
    if ( shown_count != count || strcmp( shown_connected, connected ) != 0 ) return false;
    for ( uint16_t i = 0; i < count; i++ )
    {
        if ( strcmp( shown_ssids[ i ], ( const char * )ap_records[ i ].ssid ) != 0 ) return false;
    }
    return true;
}

static bool row_pressed( void )
{
    for ( uint32_t i = 0; i < lv_obj_get_child_count( ap_list ); i++ )
    {
        if ( lv_obj_has_state( lv_obj_get_child( ap_list, i ), LV_STATE_PRESSED ) ) return true;
    }
    return false;
}

/* Call with the LVGL lock held. */
static void rows_show( uint16_t count, const char *connected )
{
    if ( rows_unchanged( count, connected ) )
    {
        for ( uint16_t i = 0; i < count; i++ )
        {
            lv_obj_t *rssi = lv_obj_get_child( lv_obj_get_child( ap_list, i ), -1 );
            lv_label_set_text_fmt( rssi, "%d dBm", ap_records[ i ].rssi );
        }
        return;
    }
    /* Deleting a row mid-press would drop the tap; the next scan retries */
    if ( row_pressed() ) return;

    int32_t scroll_y = lv_obj_get_scroll_y( ap_list );
    lv_obj_clean( ap_list );
    if ( count == 0 )
    {
        lv_obj_t *empty = lv_label_create( ap_list );
        lv_label_set_text_static( empty, "No networks found" );
        lv_obj_set_style_text_color( empty, lv_color_black(), 0 );
        lv_obj_set_style_pad_all( empty, 8, 0 );
    }
    for ( uint16_t i = 0; i < count; i++ )
    {
        add_ap_item( &ap_records[ i ], i, strcmp( connected, ( const char * )ap_records[ i ].ssid ) == 0 );
        snprintf( shown_ssids[ i ], sizeof( shown_ssids[ i ] ), "%s", ( const char * )ap_records[ i ].ssid );
    }
    shown_count = count;
    snprintf( shown_connected, sizeof( shown_connected ), "%s", connected );
    lv_obj_update_layout( ap_list );
    lv_obj_scroll_to_y( ap_list, scroll_y, LV_ANIM_OFF );
}

static void wifi_scan_once( void )
{
    if ( lvgl_port_lock( 1000 ) )
    {
        lv_label_set_text_static( scan_status, "Scanning..." );
        lvgl_port_unlock();
    }

    uint16_t count = WIFI_SCAN_RECORDS;
    esp_err_t err = core2foraws_wifi_scan( ap_records, &count );
    if ( err != ESP_OK )
    {
        ESP_LOGW( TAG, "Wi-Fi scan failed: %s", esp_err_to_name( err ) );
        if ( lvgl_port_lock( 1000 ) )
        {
            /* The radio can't scan while it is associating */
            if ( link_state() == CORE2FORAWS_WIFI_STATE_CONNECTING )
                lv_label_set_text_static( scan_status, "Connecting to saved network..." );
            else
                lv_label_set_text_fmt( scan_status, "Scan failed: %s", esp_err_to_name( err ) );
            lvgl_port_unlock();
        }
        return;
    }
    count = wifi_unique_networks( ap_records, count );

    char connected[ MAX_SSID_LEN + 1 ] = "";
    if ( link_state() == CORE2FORAWS_WIFI_STATE_CONNECTED )
        ( void )core2foraws_wifi_saved_ssid_get( connected );

    if ( !atomic_load( &scan_active ) || !lvgl_port_lock( 1000 ) ) return;
    lv_label_set_text_static( scan_status, "Scan complete" );
    rows_show( count, connected );
    lvgl_port_unlock();
    ESP_LOGI( TAG, "Scan found %u networks", count );
}

static void wifi_connect_run( void )
{
    esp_err_t err = core2foraws_wifi_connect( connect_ssid, connect_password, WIFI_CONNECT_TIMEOUT_MS );
    memset( connect_password, 0, sizeof( connect_password ) );
    ESP_LOGI( TAG, "Connect to %s: %s", connect_ssid, esp_err_to_name( err ) );

    lvgl_port_lock( 0 );
    if ( err == ESP_OK )
    {
        lv_label_set_text_fmt( scan_status, "Connected to %s", connect_ssid );
        show_get_started();
    }
    else if ( err == ESP_ERR_INVALID_STATE )
        lv_label_set_text_static( scan_status, "Wi-Fi is busy with phone setup" );
    else
    {
        lv_label_set_text_static( scan_status, "Connection failed" );
        show_connect_failed( err );
    }
    lvgl_port_unlock();
    atomic_store( &connect_busy, false );
}

static void wifi_task( void *pvParameters )
{
    ( void )pvParameters;
    for ( ; ; )
    {
        if ( atomic_exchange( &connect_requested, false ) )
        {
            wifi_connect_run();
            continue;
        }
        if ( !atomic_load( &scan_active ) )
        {
            ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
            continue;
        }
        wifi_scan_once();
        ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS( WIFI_SCAN_INTERVAL_MS ) );
    }
}

void display_wifi_tab( lv_obj_t *tv )
{
    ESP_LOGD( TAG, "Building tab" );
    lvgl_port_lock( 0 );

    lv_obj_t *wifi_tab = ui_tabview_add_tab( tv, WIFI_TAB_NAME );

    lv_obj_t *card = ui_create_card( wifi_tab, lv_color_make( 0, 82, 118 ) );
    ui_card_title( card, "Wi-Fi Networks (2.4GHz)", lv_color_white() );
    scan_status = ui_card_text( card, "Ready", lv_color_white() );
    ui_test_id( scan_status, "wifi.state" );

    ap_list = lv_obj_create( card );
    ui_test_id( ap_list, "wifi.results" );
    lv_obj_set_width( ap_list, lv_pct( 100 ) );
    lv_obj_set_flex_grow( ap_list, 1 );
    lv_obj_set_flex_flow( ap_list, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_scroll_dir( ap_list, LV_DIR_VER );
    lv_obj_set_style_pad_all( ap_list, 0, 0 );
    lv_obj_set_style_pad_row( ap_list, 0, 0 );
    lv_obj_set_style_radius( ap_list, 4, 0 );
    lv_obj_set_style_border_width( ap_list, 0, 0 );

    lv_style_init( &item_style );
    lv_style_set_bg_color( &item_style, lv_color_white() );
    lv_style_set_bg_opa( &item_style, LV_OPA_COVER );
    lv_style_set_text_color( &item_style, lv_color_black() );
    lv_style_set_border_color( &item_style, lv_color_hex( 0xdddddd ) );
    lv_style_set_border_width( &item_style, 1 );
    lv_style_set_border_side( &item_style, LV_BORDER_SIDE_BOTTOM );
    lv_style_set_pad_hor( &item_style, 8 );
    lv_style_set_pad_ver( &item_style, 6 );
    lv_style_set_pad_column( &item_style, 8 );

    lv_style_init( &item_pressed_style );
    lv_style_set_bg_color( &item_pressed_style, lv_color_hex( 0xdddddd ) );

    lvgl_port_unlock();

    if ( xTaskCreatePinnedToCore( wifi_task, "WiFiTask", 4096,
                                 NULL, 1, &wifi_handle, 1 ) != pdPASS )
    {
        wifi_handle = NULL;
        ESP_LOGE( TAG, "Failed to create WiFiTask (low internal memory)" );
    }

    char saved[ MAX_SSID_LEN + 1 ];
    if ( core2foraws_wifi_saved_ssid_get( saved ) == ESP_OK )
    {
        ESP_LOGI( TAG, "Reconnecting to saved network %s", saved );
        esp_err_t err = core2foraws_wifi_reconnect( 0 );
        if ( err != ESP_OK ) ESP_LOGW( TAG, "Reconnect failed to start: %s", esp_err_to_name( err ) );
    }
}
