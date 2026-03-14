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
    color_lock = xSemaphoreCreateMutex();

    lvgl_port_lock( 0 );

    lv_obj_t *led_bar_tab = lv_tabview_add_tab(tv, LED_BAR_TAB_NAME);
    lv_obj_set_style_pad_all( led_bar_tab, 0, 0 );

    /* Create the main body object and set background within the tab*/
    lv_obj_t *led_bar_bg = lv_obj_create( led_bar_tab );
    lv_obj_set_style_pad_all( led_bar_bg, 0, 0 );
    lv_obj_align( led_bar_bg, LV_ALIGN_TOP_LEFT, 16, 36 );
    lv_obj_set_size( led_bar_bg, 290, 190 );
    lv_obj_remove_flag( led_bar_bg, LV_OBJ_FLAG_CLICKABLE );
    
    /* Create the main body object and set background within the tab*/
    static lv_style_t bg_style;
    lv_style_init( &bg_style );
    lv_style_set_bg_color( &bg_style, lv_color_make( 236, 216, 218 ) );
    lv_obj_add_style( led_bar_bg, &bg_style, 0 );

    /* Create the title within the main body object */
    static lv_style_t title_style;
    lv_style_init( &title_style );
    lv_style_set_text_font( &title_style, LV_FONT_DEFAULT );
    lv_style_set_text_color( &title_style, lv_color_make(0,0,0) );
    lv_obj_t *tab_title_label = lv_label_create( led_bar_bg );
    lv_obj_add_style( tab_title_label, &title_style, 0 );
    lv_label_set_text_static( tab_title_label, "SK6812 LED Bars" );
    lv_obj_align( tab_title_label, LV_ALIGN_TOP_MID, 0, 10 );

    /* Create the sensor information label object */
    lv_obj_t *body_label = lv_label_create( led_bar_bg );
    lv_label_set_long_mode( body_label, LV_LABEL_LONG_WRAP );
    lv_label_set_text_static( body_label, "The ten SK6812s allow you to control each of the RGB LEDs brightness & color individually." );
    lv_obj_set_width( body_label, 252 );
    lv_obj_align_to( body_label, led_bar_bg, LV_ALIGN_TOP_LEFT, 20, 40 );

    static lv_style_t body_style;
    lv_style_init( &body_style );
    lv_style_set_text_color( &body_style, lv_color_make(0,0,0) );
    lv_obj_add_style( body_label, &body_style, 0 );

    lv_obj_t *instruction_label = lv_label_create( led_bar_bg );
    lv_label_set_text_static( instruction_label, "Drag slider to change color:" );
    lv_obj_align_to( instruction_label, body_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10 );
    lv_obj_add_style( instruction_label, &body_style, 0 );

    /* Red slider */
    lv_obj_t *red_label = lv_label_create( led_bar_bg );
    lv_label_set_text_static( red_label, "Red" );
    lv_obj_align( red_label, LV_ALIGN_BOTTOM_LEFT, 10, -32 );

    lv_obj_t *red_slider = lv_slider_create( led_bar_bg );
    lv_slider_set_range( red_slider, 0, 255 );
    lv_slider_set_value( red_slider, RED_AMAZON_ORANGE, LV_ANIM_OFF );
    lv_obj_set_size( red_slider, 60, 10 );
    lv_obj_align( red_slider, LV_ALIGN_BOTTOM_LEFT, 10, -14 );
    lv_obj_set_style_bg_color( red_slider, lv_palette_main( LV_PALETTE_RED ), LV_PART_INDICATOR );
    lv_obj_set_style_bg_color( red_slider, lv_palette_main( LV_PALETTE_RED ), LV_PART_KNOB );
    lv_obj_add_event_cb( red_slider, red_event_handler, LV_EVENT_VALUE_CHANGED, NULL );

    /* Green slider */
    lv_obj_t *green_label = lv_label_create( led_bar_bg );
    lv_label_set_text_static( green_label, "Green" );
    lv_obj_align( green_label, LV_ALIGN_BOTTOM_MID, 0, -32 );

    lv_obj_t *green_slider = lv_slider_create( led_bar_bg );
    lv_slider_set_range( green_slider, 0, 255 );
    lv_slider_set_value( green_slider, GREEN_AMAZON_ORANGE, LV_ANIM_OFF );
    lv_obj_set_size( green_slider, 60, 10 );
    lv_obj_align( green_slider, LV_ALIGN_BOTTOM_MID, 0, -14 );
    lv_obj_set_style_bg_color( green_slider, lv_palette_main( LV_PALETTE_GREEN ), LV_PART_INDICATOR );
    lv_obj_set_style_bg_color( green_slider, lv_palette_main( LV_PALETTE_GREEN ), LV_PART_KNOB );
    lv_obj_add_event_cb( green_slider, green_event_handler, LV_EVENT_VALUE_CHANGED, NULL );

    /* Blue slider */
    lv_obj_t *blue_label = lv_label_create( led_bar_bg );
    lv_label_set_text_static( blue_label, "Blue" );
    lv_obj_align( blue_label, LV_ALIGN_BOTTOM_RIGHT, -20, -32 );

    lv_obj_t *blue_slider = lv_slider_create( led_bar_bg );
    lv_slider_set_range( blue_slider, 0, 255 );
    lv_slider_set_value( blue_slider, BLUE_AMAZON_ORANGE, LV_ANIM_OFF );
    lv_obj_set_size( blue_slider, 60, 10 );
    lv_obj_align( blue_slider, LV_ALIGN_BOTTOM_RIGHT, -10, -14 );
    lv_obj_set_style_bg_color( blue_slider, lv_palette_main( LV_PALETTE_BLUE ), LV_PART_INDICATOR );
    lv_obj_set_style_bg_color( blue_slider, lv_palette_main( LV_PALETTE_BLUE ), LV_PART_KNOB );
    lv_obj_add_event_cb( blue_slider, blue_event_handler, LV_EVENT_VALUE_CHANGED, NULL );

    lvgl_port_unlock();
    
    xTaskCreatePinnedToCore( sk6812_animation_task, "sk6812AnimationTask", configMINIMAL_STACK_SIZE * 3, NULL, 1, &led_bar_animation_handle, 1 );
    xTaskCreatePinnedToCore( sk6812_solid_task, "sk6812SolidTask", configMINIMAL_STACK_SIZE * 3, NULL, 0, &led_bar_solid_handle, 1 );

}

void update_color()
{
    xSemaphoreTake( color_lock, pdMS_TO_TICKS( 10 ) );
    uint8_t current_red = red, current_green = green, current_blue = blue;
    xSemaphoreGive( color_lock );
    core2foraws_rgb_led_side_color_set( RGB_LED_SIDE_LEFT, ( current_red << 16 ) + ( current_green << 8 ) + ( current_blue ) );
    core2foraws_rgb_led_side_color_set( RGB_LED_SIDE_RIGHT, ( current_red << 16 ) + ( current_green << 8 ) + ( current_blue ) );
    core2foraws_rgb_led_write();
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
            ESP_LOGI( TAG, "Color changed to #%.2x%.2x%.2x", current_red, current_green, current_blue );
        }
        vTaskDelay( pdMS_TO_TICKS( 10 ) );
    };
    
    vTaskDelete( NULL );
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
    vTaskDelete( NULL ); // Should never get to here...
}