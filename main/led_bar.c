/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * led_bar.c
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
#include "led_bar.h"

#define RED_AMAZON_ORANGE 255
#define GREEN_AMAZON_ORANGE 153
#define BLUE_AMAZON_ORANGE 0
#define AMAZON_ORANGE 16750848 // Amazon Orange in Decimal

static SemaphoreHandle_t color_lock;

static uint8_t red = RED_AMAZON_ORANGE, green = GREEN_AMAZON_ORANGE, blue = BLUE_AMAZON_ORANGE;

static const char* TAG = LED_BAR_TAB_NAME;

static void red_event_handler(lv_event_t *e);
static void green_event_handler(lv_event_t *e);
static void blue_event_handler(lv_event_t *e);

void display_LED_bar_tab(lv_obj_t *tv)
{
    ESP_LOGD( TAG, "Building tab" );
    color_lock = xSemaphoreCreateMutex();

    lvgl_port_lock( 0 );

    lv_obj_t *led_bar_tab = ui_tabview_add_tab(tv, LED_BAR_TAB_NAME);

    /* Card with flex-column layout */
    lv_obj_t *card = ui_create_card( led_bar_tab, lv_color_make( 236, 216, 218 ) );
    ui_card_title( card, "SK6812 LED Bars", lv_color_make(0,0,0) );
    ui_card_text( card, "The ten SK6812s allow you to control each of the RGB LEDs brightness & color individually.", lv_color_make(0,0,0) );

    lv_obj_t *instruction_label = lv_label_create( card );
    lv_label_set_text_static( instruction_label, "Drag slider to change color:" );
    lv_obj_set_style_text_color( instruction_label, lv_color_make(0,0,0), 0 );

    /* Slider row: R | G | B — horizontal flex */
    lv_obj_t *slider_row = lv_obj_create( card );
    lv_obj_remove_style_all( slider_row );
    lv_obj_set_width( slider_row, lv_pct( 100 ) );
    lv_obj_set_height( slider_row, LV_SIZE_CONTENT );
    lv_obj_set_layout( slider_row, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( slider_row, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( slider_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_column( slider_row, 4, 0 );
    lv_obj_set_flex_grow( slider_row, 1 );

    /* Helper: create a labeled slider column */
    struct { const char *name; uint8_t val; lv_palette_t pal; lv_event_cb_t cb; } sliders[] = {
        { "Red",   RED_AMAZON_ORANGE,   LV_PALETTE_RED,   red_event_handler },
        { "Green", GREEN_AMAZON_ORANGE, LV_PALETTE_GREEN, green_event_handler },
        { "Blue",  BLUE_AMAZON_ORANGE,  LV_PALETTE_BLUE,  blue_event_handler },
    };
    for ( int i = 0; i < 3; i++ )
    {
        lv_obj_t *col = lv_obj_create( slider_row );
        lv_obj_remove_style_all( col );
        lv_obj_set_size( col, LV_SIZE_CONTENT, LV_SIZE_CONTENT );
        lv_obj_set_layout( col, LV_LAYOUT_FLEX );
        lv_obj_set_flex_flow( col, LV_FLEX_FLOW_COLUMN );
        lv_obj_set_flex_align( col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
        lv_obj_set_style_pad_row( col, 4, 0 );

        lv_obj_t *lbl = lv_label_create( col );
        lv_label_set_text_static( lbl, sliders[i].name );

        lv_obj_t *slider = lv_slider_create( col );
        lv_slider_set_range( slider, 0, 255 );
        lv_slider_set_value( slider, sliders[i].val, LV_ANIM_OFF );
        lv_obj_set_size( slider, 60, 10 );
        lv_obj_set_style_bg_color( slider, lv_palette_main( sliders[i].pal ), LV_PART_INDICATOR );
        lv_obj_set_style_bg_color( slider, lv_palette_main( sliders[i].pal ), LV_PART_KNOB );
        lv_obj_add_event_cb( slider, sliders[i].cb, LV_EVENT_VALUE_CHANGED, NULL );
    }

    lvgl_port_unlock();
    
    if ( xTaskCreatePinnedToCore( sk6812_animation_task, "sk6812AnimationTask", configMINIMAL_STACK_SIZE * 3, NULL, 1, &led_bar_animation_handle, 1 ) != pdPASS )
        ESP_LOGE( TAG, "Failed to create sk6812AnimationTask (low internal memory)" );
    if ( xTaskCreatePinnedToCore( sk6812_solid_task, "sk6812SolidTask", configMINIMAL_STACK_SIZE * 3, NULL, 0, &led_bar_solid_handle, 1 ) != pdPASS )
        ESP_LOGE( TAG, "Failed to create sk6812SolidTask (low internal memory)" );
}

static void red_event_handler( lv_event_t *e )
{
    lv_obj_t *slider = lv_event_get_target( e );
    xSemaphoreTake( color_lock, pdMS_TO_TICKS( 10 ) );
    red = ( uint8_t )lv_slider_get_value( slider );
    xSemaphoreGive( color_lock );
}

static void green_event_handler( lv_event_t *e )
{
    lv_obj_t *slider = lv_event_get_target( e );
    xSemaphoreTake( color_lock, pdMS_TO_TICKS( 10 ) );
    green = ( uint8_t )lv_slider_get_value( slider );
    xSemaphoreGive( color_lock );
}

static void blue_event_handler( lv_event_t *e )
{
    lv_obj_t *slider = lv_event_get_target( e );
    xSemaphoreTake( color_lock, pdMS_TO_TICKS( 10 ) );
    blue = ( uint8_t )lv_slider_get_value( slider );
    xSemaphoreGive( color_lock );
}

void sk6812_solid_task( void *pvParameters )
{
    vTaskSuspend( NULL );
    xSemaphoreTake( color_lock, pdMS_TO_TICKS( 10 ) );
    uint8_t current_red = red, current_green = green, current_blue = blue;
    xSemaphoreGive( color_lock );
    core2foraws_rgb_led_side_color_set( RGB_LED_SIDE_LEFT, ( current_red << 16 ) + ( current_green << 8 ) + ( current_blue ) );
    core2foraws_rgb_led_side_color_set( RGB_LED_SIDE_RIGHT, ( current_red << 16 ) + ( current_green << 8 ) + ( current_blue ) );
    core2foraws_rgb_led_write();
    
    while( 1 )
    {
        if ( ( current_red != red ) || ( current_green != green ) || ( current_blue != blue ) )
        {
            xSemaphoreTake( color_lock, pdMS_TO_TICKS( 10 ) );
            current_red = red, current_green = green, current_blue = blue;
            xSemaphoreGive( color_lock );
            core2foraws_rgb_led_side_color_set( RGB_LED_SIDE_LEFT, ( current_red << 16 ) + ( current_green << 8 ) + ( current_blue ) );
            core2foraws_rgb_led_side_color_set( RGB_LED_SIDE_RIGHT, ( current_red << 16 ) + ( current_green << 8 ) + ( current_blue ) );
            core2foraws_rgb_led_write();
            ESP_LOGD( TAG, "Color changed to #%.2x%.2x%.2x", current_red, current_green, current_blue );
        }
        vTaskDelay( pdMS_TO_TICKS( 10 ) );
    };
}

void sk6812_animation_task( void *pvParameters )
{
    while ( 1 )
    {
        core2foraws_rgb_led_clear();
        core2foraws_rgb_led_write();

        for ( uint8_t i = 0; i < 10; i++ )
        {
            core2foraws_rgb_led_single_color_set( i, AMAZON_ORANGE );
            core2foraws_rgb_led_write();
            vTaskDelay( pdMS_TO_TICKS( 70 ) );
        }

        for ( uint8_t i = 0; i < 10; i++ )
        {
            core2foraws_rgb_led_single_color_set( i, 0x000000 );
            core2foraws_rgb_led_write();
            vTaskDelay( pdMS_TO_TICKS( 70 ) );
        }

        core2foraws_rgb_led_side_color_set( RGB_LED_SIDE_LEFT, 0x232f3e );
        core2foraws_rgb_led_side_color_set( RGB_LED_SIDE_RIGHT, 0xffffff );
        core2foraws_rgb_led_write();

        for ( uint8_t i = 40; i > 0; i-- )
        {
            core2foraws_rgb_led_brightness_set(i);
            core2foraws_rgb_led_write();
            vTaskDelay( pdMS_TO_TICKS( 25 ) );
        }

        core2foraws_rgb_led_brightness_set( 20 );
    }
}