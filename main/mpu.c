/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * mpu.c
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
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "mpu.h"

/* 
If you want to use Sebastian Madgwick's algorithym to calculate roll, pitch, yaw, add the libs from: 
https://x-io.co.uk/open-source-imu-and-ahrs-algorithms/

Note that since there is no magnometer (and it wouldn't work correctly due to magnets in the housing) 
there will be a drift in the Yaw. The drift cannot be correct without creating a custom algorithm to 
serve as a filter for the drift. Uncomment the commented code below. You will need to make modifications
to the imu-and-ahrs-algorithms library for it to write to the pitch, yaw, roll pointers.
    
Note: Sebastian Madgwick's algorithym implementation library is GPL licensed.
*/

// #include "MahonyAHRS.h"

// #define DEGREES_TO_RADIANS M_PI/180
// #define RADIANS_TO_DEGREES 180/M_PI

TaskHandle_t MPU_handle;

static const char *TAG = MPU_TAB_NAME;

static lv_obj_t *needle_x;
static lv_obj_t *needle_y;
static lv_obj_t *needle_z;

void display_mpu_tab(lv_obj_t *tv)
{
    ESP_LOGD( TAG, "Building tab" );
    lvgl_port_lock( 0 );
    
    lv_obj_t *mpu_tab = ui_tabview_add_tab(tv, MPU_TAB_NAME);

    /* Single card, with title + side-by-side content + legend */
    lv_obj_t *card = ui_create_card( mpu_tab, lv_color_make( 169, 0, 103 ) );
    ui_card_title( card, "MPU6886 IMU Sensor", lv_color_make(255,255,255) );
    lv_obj_set_style_pad_row( card, 8, 0 );

    /* Content row: slightly more space to the description, slightly smaller gauge */
    lv_obj_t *content_row = lv_obj_create( card );
    lv_obj_remove_style_all( content_row );
    lv_obj_set_width( content_row, lv_pct( 100 ) );
    lv_obj_set_height( content_row, 104 );
    lv_obj_set_layout( content_row, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( content_row, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( content_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_column( content_row, 8, 0 );

    /* Description area: wider than before so the copy wraps less aggressively */
    lv_obj_t *copy_wrap = lv_obj_create( content_row );
    lv_obj_remove_style_all( copy_wrap );
    lv_obj_set_size( copy_wrap, 154, lv_pct( 100 ) );
    lv_obj_set_layout( copy_wrap, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( copy_wrap, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( copy_wrap, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START );

    lv_obj_t *body_label = lv_label_create( copy_wrap );
    lv_label_set_long_mode( body_label, LV_LABEL_LONG_WRAP );
    lv_label_set_text_static( body_label,
                              "The Inertial\n"
                              "Measurement\n"
                              "Unit (IMU)\n"
                              "senses the\n"
                              "motion of the\n"
                              "device." );
    lv_obj_set_width( body_label, lv_pct( 100 ) );
    lv_obj_set_style_text_color( body_label, lv_color_make(255,255,255), 0 );
    lv_obj_set_style_text_line_space( body_label, 0, 0 );

    /* Gauge area: slightly smaller with much smaller scale text */
    lv_obj_t *meter_wrap = lv_obj_create( content_row );
    lv_obj_remove_style_all( meter_wrap );
    lv_obj_set_size( meter_wrap, 110, lv_pct( 100 ) );
    lv_obj_set_layout( meter_wrap, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( meter_wrap, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( meter_wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );

    lv_obj_t *meter = lv_scale_create( meter_wrap );
    lv_obj_remove_flag( meter, LV_OBJ_FLAG_CLICKABLE );
    lv_obj_set_size( meter, 94, 94 );
    lv_scale_set_mode( meter, LV_SCALE_MODE_ROUND_INNER );
    lv_scale_set_range( meter, -400, 400 );
    lv_scale_set_angle_range( meter, 300 );
    lv_scale_set_rotation( meter, 120 );
    lv_scale_set_total_tick_count( meter, 11 );
    lv_scale_set_major_tick_every( meter, 2 );

    lv_obj_set_style_length( meter, 8, LV_PART_INDICATOR );
    lv_obj_set_style_length( meter, 4, LV_PART_ITEMS );
    lv_obj_set_style_line_width( meter, 1, LV_PART_ITEMS );
    lv_obj_set_style_line_width( meter, 1, LV_PART_INDICATOR );
    lv_obj_set_style_text_font( meter, &lv_font_montserrat_8, 0 );
    lv_obj_set_style_text_color( meter, lv_color_make( 255, 255, 255 ), 0 );
    lv_obj_set_style_text_opa( meter, LV_OPA_90, 0 );
    lv_obj_set_style_text_letter_space( meter, -1, 0 );

    needle_x = lv_line_create( meter );
    lv_obj_set_style_line_color( needle_x, lv_palette_main( LV_PALETTE_RED ), 0 );
    lv_obj_set_style_line_width( needle_x, 2, 0 );

    needle_y = lv_line_create( meter );
    lv_obj_set_style_line_color( needle_y, lv_palette_main( LV_PALETTE_GREEN ), 0 );
    lv_obj_set_style_line_width( needle_y, 2, 0 );

    needle_z = lv_line_create( meter );
    lv_obj_set_style_line_color( needle_z, lv_palette_main( LV_PALETTE_BLUE ), 0 );
    lv_obj_set_style_line_width( needle_z, 2, 0 );

    /* Compact legend row at bottom of card */
    lv_obj_t *lgnd_bg = lv_obj_create( card );
    lv_obj_set_size( lgnd_bg, lv_pct( 80 ), 24 );
    lv_obj_set_style_bg_color( lgnd_bg, lv_color_make(255,255,255), 0 );
    lv_obj_set_style_bg_opa( lgnd_bg, LV_OPA_COVER, 0 );
    lv_obj_set_style_border_width( lgnd_bg, 0, 0 );
    lv_obj_set_style_radius( lgnd_bg, 4, 0 );
    lv_obj_set_layout( lgnd_bg, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( lgnd_bg, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( lgnd_bg, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_all( lgnd_bg, 2, 0 );
    lv_obj_set_style_margin_top( lgnd_bg, -8, 0 );

    lv_obj_t *legend_label_x = lv_label_create( lgnd_bg );
    lv_label_set_text_static( legend_label_x, "Rot_X" );
    lv_obj_set_style_text_color( legend_label_x, lv_color_hex(0xff0000), 0 );

    lv_obj_t *legend_label_y = lv_label_create( lgnd_bg );
    lv_label_set_text_static( legend_label_y, "Rot_Y" );
    lv_obj_set_style_text_color( legend_label_y, lv_color_hex(0x008000), 0 );

    lv_obj_t *legend_label_z = lv_label_create( lgnd_bg );
    lv_label_set_text_static( legend_label_z, "Rot_Z" );
    lv_obj_set_style_text_color( legend_label_z, lv_color_hex(0x0000ff), 0 );

    lvgl_port_unlock();
    
    xTaskCreatePinnedToCore( MPU_task, "MPUTask", 2048, ( void * ) meter, 1, &MPU_handle, 1 );
}

void MPU_task( void *pvParameters )
{
    float calib_gx = 0.00;
    float calib_gy = 0.00;
    float calib_gz = 0.00;

    float calib_ax = 0.00;
    float calib_ay = 0.00;
    float calib_az = 0.00;

    core2foraws_motion_accel_get( &calib_ax, &calib_ay, &calib_az );
    core2foraws_motion_gyro_get( &calib_gx, &calib_gy, &calib_gz );
    
    vTaskSuspend( NULL );

    for ( ; ; )
    {
        float gx, gy, gz;
        float ax, ay, az;
        core2foraws_motion_accel_get( &ax, &ay, &az );
        core2foraws_motion_gyro_get( &gx, &gy, &gz );

        ESP_LOGD( TAG, "Raw Accel: X-%.2f Y-%.2f Z-%.2f | Gyro: X-%.2f Y-%.2f Z-%.2f", ax, ay, az, gx, gy, gz );

        lv_obj_t *meter = ( lv_obj_t * )pvParameters;
        
        /* Bounded wait with padding; skip this needle update if the LVGL
         * render loop is busy rather than blocking this task forever. */
        if ( lvgl_port_lock( 1000 ) )
        {
            lv_scale_set_line_needle_value( meter, needle_x, 40, ( int ) ( gx-calib_gx ));
            lv_scale_set_line_needle_value( meter, needle_y, 40, ( int ) ( gy-calib_gy ));
            lv_scale_set_line_needle_value( meter, needle_z, 40, ( int ) ( gz-calib_gz ));
            lvgl_port_unlock(); 
        }
        else
        {
            ESP_LOGW( TAG, "LVGL lock timeout; skipping IMU needle update" );
        }
        
        vTaskDelay( pdMS_TO_TICKS( 30 ) );
    }
}
