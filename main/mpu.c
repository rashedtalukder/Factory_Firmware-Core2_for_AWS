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
    lvgl_port_lock( 0 );
    
    lv_obj_t *mpu_tab = lv_tabview_add_tab(tv, MPU_TAB_NAME);
    lv_obj_set_style_pad_all( mpu_tab, 0, 0 );
    /* Create the main body object and set background within the tab*/
    static lv_style_t bg_style;
    lv_obj_t *mpu_bg = lv_obj_create( mpu_tab );
    lv_obj_set_style_pad_all( mpu_bg, 0, 0 );
    lv_obj_align( mpu_bg, LV_ALIGN_TOP_LEFT, 16, 36 );
    lv_obj_set_size( mpu_bg, 290, 190 );
    lv_obj_remove_flag( mpu_bg, LV_OBJ_FLAG_CLICKABLE );
    lv_style_init( &bg_style );
    lv_style_set_bg_color( &bg_style, lv_color_make( 169, 0, 103 ) );
    lv_obj_add_style( mpu_bg, &bg_style, 0 );

    /* Create the title within the main body object */
    static lv_style_t title_style;
    lv_style_init( &title_style );
    lv_style_set_text_font( &title_style, LV_FONT_DEFAULT );
    lv_style_set_text_color( &title_style, lv_color_make(255,255,255) );
    lv_obj_t *tab_title_label = lv_label_create( mpu_bg );
    lv_obj_add_style( tab_title_label, &title_style, 0 );
    lv_label_set_text_static( tab_title_label, "MPU6886 IMU Sensor" );
    lv_obj_align( tab_title_label, LV_ALIGN_TOP_MID, 0, 10 );

    /* Create the sensor information label object */
    lv_obj_t *body_label = lv_label_create( mpu_bg );
    lv_label_set_long_mode( body_label, LV_LABEL_LONG_WRAP );
    lv_label_set_text_static( body_label, "The Inertial Measurement Unit (IMU) senses the motion of the device." );
    lv_obj_set_width( body_label, 120 );
    lv_obj_align( body_label, LV_ALIGN_LEFT_MID, 20, 0 );

    static lv_style_t body_style;
    lv_style_init( &body_style );
    lv_style_set_text_color( &body_style, lv_color_make(255,255,255) );
    lv_obj_add_style( body_label, &body_style, 0 );

    /* Create the sensor color legend */
    lv_obj_t *lgnd_bg = lv_obj_create( mpu_bg );
    lv_obj_set_size( lgnd_bg, 200, 24 );
    lv_obj_align( lgnd_bg, LV_ALIGN_BOTTOM_MID, 0, -10 );
    lv_obj_set_style_bg_color( lgnd_bg, lv_color_make(255,255,255), 0 );
    lv_obj_t *legend_label_x = lv_label_create( lgnd_bg );
    lv_label_set_text_static( legend_label_x, "Rot_X" );
    lv_obj_set_style_text_color( legend_label_x, lv_color_hex(0xff0000), 0 );
    lv_obj_align( legend_label_x, LV_ALIGN_LEFT_MID, 4, 0 );
    lv_obj_t *legend_label_y = lv_label_create( lgnd_bg );
    lv_label_set_text_static( legend_label_y, "Rot_Y" );
    lv_obj_set_style_text_color( legend_label_y, lv_color_hex(0x008000), 0 );
    lv_obj_align( legend_label_y, LV_ALIGN_CENTER, 0, 0 );
    lv_obj_t *legend_label_z = lv_label_create( lgnd_bg );
    lv_label_set_text_static( legend_label_z, "Rot_Z" );
    lv_obj_set_style_text_color( legend_label_z, lv_color_hex(0x0000ff), 0 );
    lv_obj_align( legend_label_z, LV_ALIGN_RIGHT_MID, -4, 0 );
    
    /* Create a scale (replaces meter in LVGL 9) */
    lv_obj_t *meter = lv_scale_create( mpu_bg );
    lv_obj_remove_flag( meter, LV_OBJ_FLAG_CLICKABLE );
    lv_obj_set_size( meter, 106, 106 );
    lv_scale_set_mode( meter, LV_SCALE_MODE_ROUND_INNER );
    lv_scale_set_range( meter, -400, 400 );
    lv_scale_set_angle_range( meter, 300 );
    lv_scale_set_rotation( meter, 120 );
    lv_scale_set_total_tick_count( meter, 11 );
    lv_scale_set_major_tick_every( meter, 2 );
    lv_obj_set_style_length( meter, 10, LV_PART_INDICATOR );
    lv_obj_set_style_length( meter, 5, LV_PART_ITEMS );

    needle_x = lv_line_create( meter );
    lv_obj_set_style_line_color( needle_x, lv_palette_main( LV_PALETTE_RED ), 0 );
    lv_obj_set_style_line_width( needle_x, 2, 0 );
    needle_y = lv_line_create( meter );
    lv_obj_set_style_line_color( needle_y, lv_palette_main( LV_PALETTE_GREEN ), 0 );
    lv_obj_set_style_line_width( needle_y, 2, 0 );
    needle_z = lv_line_create( meter );
    lv_obj_set_style_line_color( needle_z, lv_palette_main( LV_PALETTE_BLUE ), 0 );
    lv_obj_set_style_line_width( needle_z, 2, 0 );

    lv_obj_align( meter, LV_ALIGN_RIGHT_MID, -20, 0 );
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

        ESP_LOGI( TAG, "Raw Accel: X-%.6f Y-%.6f Z-%.6f | Gyro: X-%.6f Y-%.6fZ- %.6f", ax, ay, az, gx, gy, gz );

        lv_obj_t *meter = ( lv_obj_t * )pvParameters;
        
        lvgl_port_lock( 0 );
        lv_scale_set_line_needle_value( meter, needle_x, 40, ( int ) ( gx-calib_gx ));
        lv_scale_set_line_needle_value( meter, needle_y, 40, ( int ) ( gy-calib_gy ));
        lv_scale_set_line_needle_value( meter, needle_z, 40, ( int ) ( gz-calib_gz ));
        lvgl_port_unlock(); 
        
        vTaskDelay( pdMS_TO_TICKS( 30 ) );
    }
    vTaskDelete( NULL ); // Should never get to here...
}