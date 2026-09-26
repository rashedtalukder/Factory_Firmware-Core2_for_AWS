/*
 * AWS IoT Kit - M5Stack Core2
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
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "mpu.h"

TaskHandle_t MPU_handle;

static const char *TAG = MPU_TAB_NAME;

#define MPU_CARD_COLOR          0x161E2D
#define MPU_TEXT_COLOR          0xD5DBE3
#define MPU_EDGE_COLOR          0x0B111C

#define CUBE_AREA_WIDTH         140
/* Pixels per unit at the cube's center depth; sized so any pose fits the area. */
#define CUBE_SCALE              30.0f
#define CUBE_CAMERA_DISTANCE    6.0f

#define SAMPLE_PERIOD_MS        10
#define REDRAW_EVERY_SAMPLES    3
#define MAX_DT_S                0.05f
#define DEG_TO_RAD              ( ( float )M_PI / 180.0f )

#define GYRO_CALIBRATION_SAMPLES 64
/* Gravity correction gain (rad/s per unit error); stronger when at rest. */
#define GRAVITY_GAIN_MOVING     1.0f
#define GRAVITY_GAIN_STILL      5.0f
#define ACCEL_TRUST_G           0.15f
#define STILL_GYRO_DPS          3.0f
#define STILL_ACCEL_G           0.05f
#define STILL_SAMPLES           50
#define BIAS_TRACK_RATE         0.02f

typedef struct
{
    float w, x, y, z;
} quat_t;

/* Cube vertex i has x = bit0, y = bit1, z = bit2 (set = +1). Faces wind CCW from outside. */
static const uint8_t cube_faces[ 6 ][ 4 ] =
{
    { 4, 5, 7, 6 }, /* +Z, screen side */
    { 0, 2, 3, 1 }, /* -Z */
    { 1, 3, 7, 5 }, /* +X */
    { 0, 4, 6, 2 }, /* -X */
    { 2, 6, 7, 3 }, /* +Y */
    { 0, 1, 5, 4 }, /* -Y */
};
static const int8_t face_normals[ 6 ][ 3 ] =
{
    { 0, 0, 1 }, { 0, 0, -1 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 },
};
static const uint32_t face_colors[ 6 ] =
{
    0xFF9900, 0x8C4FFF, 0x00A8E1, 0x3EB489, 0xE8EDF2, 0xE0245E,
};

/* Camera tilt Rx(20 deg) * Ry(-30 deg) so the front, top, and right faces show at rest. */
static const float view_matrix[ 3 ][ 3 ] =
{
    {  0.8660f, 0.0000f, -0.5000f },
    { -0.1710f, 0.9397f, -0.2962f },
    {  0.4698f, 0.3420f,  0.8138f },
};
static const float light_dir[ 3 ] = { -0.35f, 0.55f, 0.76f };

static lv_obj_t *cube_obj;
/* Written by MPU_task and read by the draw callback, both under the LVGL lock. */
static float cube_matrix[ 3 ][ 3 ];
static atomic_bool mpu_active;

static void cube_matrix_set( float rotation[ 3 ][ 3 ] )
{
    for ( int r = 0; r < 3; r++ )
    {
        for ( int c = 0; c < 3; c++ )
        {
            cube_matrix[ r ][ c ] = view_matrix[ r ][ 0 ] * rotation[ 0 ][ c ] +
                                    view_matrix[ r ][ 1 ] * rotation[ 1 ][ c ] +
                                    view_matrix[ r ][ 2 ] * rotation[ 2 ][ c ];
        }
    }
}

static void cube_draw_cb( lv_event_t *e )
{
    lv_obj_t *obj = lv_event_get_target_obj( e );
    lv_layer_t *layer = lv_event_get_layer( e );
    lv_area_t area;
    lv_obj_get_coords( obj, &area );
    float cx = area.x1 + lv_area_get_width( &area ) / 2.0f;
    float cy = area.y1 + lv_area_get_height( &area ) / 2.0f;

    float px[ 8 ], py[ 8 ];
    lv_point_precise_t pts[ 8 ];
    for ( int i = 0; i < 8; i++ )
    {
        float v[ 3 ] = { ( i & 1 ) ? 1.0f : -1.0f, ( i & 2 ) ? 1.0f : -1.0f, ( i & 4 ) ? 1.0f : -1.0f };
        float x = cube_matrix[ 0 ][ 0 ] * v[ 0 ] + cube_matrix[ 0 ][ 1 ] * v[ 1 ] + cube_matrix[ 0 ][ 2 ] * v[ 2 ];
        float y = cube_matrix[ 1 ][ 0 ] * v[ 0 ] + cube_matrix[ 1 ][ 1 ] * v[ 1 ] + cube_matrix[ 1 ][ 2 ] * v[ 2 ];
        float z = cube_matrix[ 2 ][ 0 ] * v[ 0 ] + cube_matrix[ 2 ][ 1 ] * v[ 1 ] + cube_matrix[ 2 ][ 2 ] * v[ 2 ];
        float s = CUBE_SCALE * CUBE_CAMERA_DISTANCE / ( CUBE_CAMERA_DISTANCE - z );
        px[ i ] = cx + x * s;
        py[ i ] = cy - y * s;
        pts[ i ].x = ( int32_t )lroundf( px[ i ] );
        pts[ i ].y = ( int32_t )lroundf( py[ i ] );
    }

    bool visible[ 6 ];
    lv_draw_triangle_dsc_t tri;
    lv_draw_triangle_dsc_init( &tri );
    tri.opa = LV_OPA_COVER;
    lv_draw_line_dsc_t seam;
    lv_draw_line_dsc_init( &seam );
    seam.width = 2;

    for ( int f = 0; f < 6; f++ )
    {
        const uint8_t *q = cube_faces[ f ];
        /* Screen y points down, so outward-facing (CCW) faces have negative area. */
        float cross = ( px[ q[ 1 ] ] - px[ q[ 0 ] ] ) * ( py[ q[ 2 ] ] - py[ q[ 0 ] ] ) -
                      ( px[ q[ 2 ] ] - px[ q[ 0 ] ] ) * ( py[ q[ 1 ] ] - py[ q[ 0 ] ] );
        visible[ f ] = cross < 0.0f;
        if ( !visible[ f ] )
            continue;

        float light = 0.0f;
        for ( int r = 0; r < 3; r++ )
        {
            float n = cube_matrix[ r ][ 0 ] * face_normals[ f ][ 0 ] +
                      cube_matrix[ r ][ 1 ] * face_normals[ f ][ 1 ] +
                      cube_matrix[ r ][ 2 ] * face_normals[ f ][ 2 ];
            light += n * light_dir[ r ];
        }
        float shade = 0.45f + 0.55f * fmaxf( light, 0.0f );
        tri.color = lv_color_mix( lv_color_hex( face_colors[ f ] ), lv_color_black(), ( uint8_t )( shade * 255.0f ) );

        tri.p[ 0 ] = pts[ q[ 0 ] ];
        tri.p[ 1 ] = pts[ q[ 1 ] ];
        tri.p[ 2 ] = pts[ q[ 2 ] ];
        lv_draw_triangle( layer, &tri );
        tri.p[ 1 ] = pts[ q[ 2 ] ];
        tri.p[ 2 ] = pts[ q[ 3 ] ];
        lv_draw_triangle( layer, &tri );

        /* Hide the anti-aliased gap between the face's two triangles. */
        seam.color = tri.color;
        seam.p1 = pts[ q[ 0 ] ];
        seam.p2 = pts[ q[ 2 ] ];
        lv_draw_line( layer, &seam );
    }

    lv_draw_line_dsc_t edge;
    lv_draw_line_dsc_init( &edge );
    edge.color = lv_color_hex( MPU_EDGE_COLOR );
    edge.width = 2;
    edge.round_start = 1;
    edge.round_end = 1;
    for ( int f = 0; f < 6; f++ )
    {
        if ( !visible[ f ] )
            continue;
        for ( int k = 0; k < 4; k++ )
        {
            edge.p1 = pts[ cube_faces[ f ][ k ] ];
            edge.p2 = pts[ cube_faces[ f ][ ( k + 1 ) % 4 ] ];
            lv_draw_line( layer, &edge );
        }
    }
}

void mpu_set_active( bool active )
{
    atomic_store( &mpu_active, active );
    if( active && MPU_handle != NULL )
    {
        xTaskNotifyGive( MPU_handle );
    }
}

void display_mpu_tab(lv_obj_t *tv)
{
    ESP_LOGD( TAG, "Building tab" );
    lvgl_port_lock( 0 );
    
    lv_obj_t *mpu_tab = ui_tabview_add_tab(tv, MPU_TAB_NAME);

    lv_obj_t *card = ui_create_card( mpu_tab, lv_color_hex( MPU_CARD_COLOR ) );
    ui_card_title( card, "MPU6886 IMU Sensor", lv_color_white() );
    lv_obj_set_style_pad_row( card, 8, 0 );

    lv_obj_t *content_row = lv_obj_create( card );
    lv_obj_remove_style_all( content_row );
    lv_obj_set_width( content_row, lv_pct( 100 ) );
    lv_obj_set_flex_grow( content_row, 1 );
    lv_obj_set_layout( content_row, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( content_row, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( content_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_column( content_row, 8, 0 );

    lv_obj_t *body_label = lv_label_create( content_row );
    lv_label_set_long_mode( body_label, LV_LABEL_LONG_MODE_WRAP );
    lv_label_set_text_static( body_label,
                              "The IMU senses the device's motion. "
                              "Tilt or turn it to rotate the cube." );
    lv_obj_set_flex_grow( body_label, 1 );
    lv_obj_set_style_text_color( body_label, lv_color_hex( MPU_TEXT_COLOR ), 0 );

    float identity[ 3 ][ 3 ] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    cube_matrix_set( identity );

    cube_obj = lv_obj_create( content_row );
    lv_obj_remove_style_all( cube_obj );
    lv_obj_set_size( cube_obj, CUBE_AREA_WIDTH, lv_pct( 100 ) );
    lv_obj_set_clickable( cube_obj, false );
    lv_obj_add_event_cb( cube_obj, cube_draw_cb, LV_EVENT_DRAW_MAIN, NULL );

    lvgl_port_unlock();
    
    /* 2 KB left too little headroom for esp_log's vprintf on error paths. */
    if ( xTaskCreatePinnedToCore( MPU_task, "MPUTask", 3072, NULL,
                                 1, &MPU_handle, 1 ) != pdPASS )
    {
        MPU_handle = NULL;
        ESP_LOGE( TAG, "Failed to create IMU task" );
    }
}

static quat_t quat_mul( quat_t a, quat_t b )
{
    return ( quat_t ){
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

static void quat_normalize( quat_t *q )
{
    float n = sqrtf( q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z );
    q->w /= n;
    q->x /= n;
    q->y /= n;
    q->z /= n;
}

static void quat_to_matrix( quat_t q, float m[ 3 ][ 3 ] )
{
    m[ 0 ][ 0 ] = 1 - 2 * ( q.y * q.y + q.z * q.z );
    m[ 0 ][ 1 ] = 2 * ( q.x * q.y - q.w * q.z );
    m[ 0 ][ 2 ] = 2 * ( q.x * q.z + q.w * q.y );
    m[ 1 ][ 0 ] = 2 * ( q.x * q.y + q.w * q.z );
    m[ 1 ][ 1 ] = 1 - 2 * ( q.x * q.x + q.z * q.z );
    m[ 1 ][ 2 ] = 2 * ( q.y * q.z - q.w * q.x );
    m[ 2 ][ 0 ] = 2 * ( q.x * q.z - q.w * q.y );
    m[ 2 ][ 1 ] = 2 * ( q.y * q.z + q.w * q.x );
    m[ 2 ][ 2 ] = 1 - 2 * ( q.x * q.x + q.y * q.y );
}

/* Tilt from gravity with zero heading; the accelerometer cannot observe yaw. */
static quat_t quat_from_gravity( float ax, float ay, float az )
{
    float roll = atan2f( ay, az );
    float pitch = atan2f( -ax, sqrtf( ay * ay + az * az ) );
    float cr = cosf( roll / 2 ), sr = sinf( roll / 2 );
    float cp = cosf( pitch / 2 ), sp = sinf( pitch / 2 );
    return ( quat_t ){ cr * cp, sr * cp, cr * sp, -sr * sp };
}

void MPU_task( void *pvParameters )
{
    ( void )pvParameters;
    float bias[ 3 ] = { 0 };
    esp_err_t err = ESP_OK;

    /* Startup zero-rate estimate; refined later whenever the device is at rest. */
    int calib_count = 0;
    for ( int i = 0; i < GYRO_CALIBRATION_SAMPLES; i++ )
    {
        float g[ 3 ];
        err = core2foraws_motion_gyro_get( &g[ 0 ], &g[ 1 ], &g[ 2 ] );
        if ( err != ESP_OK )
            break;
        for ( int k = 0; k < 3; k++ )
            bias[ k ] += g[ k ];
        calib_count++;
        vTaskDelay( 1 );
    }
    for ( int k = 0; calib_count > 0 && k < 3; k++ )
        bias[ k ] /= calib_count;
    if ( err != ESP_OK )
        ESP_LOGW( TAG, "IMU calibration read failed: %s", esp_err_to_name( err ) );

    quat_t q = { 1, 0, 0, 0 };
    quat_t q_ref_inv = { 1, 0, 0, 0 };
    bool need_seed = true;
    int still_count = 0;
    int redraw_count = 0;
    int64_t last_us = 0;
    TickType_t last_wake = xTaskGetTickCount();

    for ( ; ; )
    {
        if ( !atomic_load( &mpu_active ) )
        {
            while ( !atomic_load( &mpu_active ) )
                ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
            need_seed = true;
            last_wake = xTaskGetTickCount();
        }

        float a[ 3 ], g[ 3 ];
        err = core2foraws_motion_accel_gyro_get( &a[ 0 ], &a[ 1 ], &a[ 2 ], &g[ 0 ], &g[ 1 ], &g[ 2 ] );
        if ( err != ESP_OK )
        {
            ESP_LOGW( TAG, "IMU read failed: %s", esp_err_to_name( err ) );
            vTaskDelay( pdMS_TO_TICKS( 1000 ) );
            need_seed = true;
            last_wake = xTaskGetTickCount();
            continue;
        }
        int64_t now_us = esp_timer_get_time();
        float a_norm = sqrtf( a[ 0 ] * a[ 0 ] + a[ 1 ] * a[ 1 ] + a[ 2 ] * a[ 2 ] );

        if ( need_seed )
        {
            /* Each visit starts from the current pose, so the cube opens in its reference view. */
            q = quat_from_gravity( a[ 0 ], a[ 1 ], a[ 2 ] );
            q_ref_inv = ( quat_t ){ q.w, -q.x, -q.y, -q.z };
            last_us = now_us;
            still_count = 0;
            redraw_count = 0;
            need_seed = false;
            xTaskDelayUntil( &last_wake, pdMS_TO_TICKS( SAMPLE_PERIOD_MS ) );
            continue;
        }

        float dt = fminf( ( now_us - last_us ) * 1e-6f, MAX_DT_S );
        last_us = now_us;

        float rate[ 3 ];
        bool calm = fabsf( a_norm - 1.0f ) < STILL_ACCEL_G;
        for ( int k = 0; k < 3; k++ )
        {
            rate[ k ] = g[ k ] - bias[ k ];
            calm = calm && fabsf( rate[ k ] ) < STILL_GYRO_DPS;
        }
        still_count = calm ? still_count + 1 : 0;
        bool still = still_count >= STILL_SAMPLES;

        /* At rest the true rate is zero: learn the bias (the only yaw drift fix without a
         * magnetometer) and hold the pose instead of integrating residual noise. */
        for ( int k = 0; k < 3; k++ )
        {
            if ( still )
            {
                bias[ k ] += BIAS_TRACK_RATE * rate[ k ];
                rate[ k ] = 0.0f;
            }
            rate[ k ] *= DEG_TO_RAD;
        }

        /* Pull roll and pitch toward gravity unless linear acceleration dominates. */
        if ( fabsf( a_norm - 1.0f ) < ACCEL_TRUST_G )
        {
            float ux = a[ 0 ] / a_norm, uy = a[ 1 ] / a_norm, uz = a[ 2 ] / a_norm;
            float vx = 2 * ( q.x * q.z - q.w * q.y );
            float vy = 2 * ( q.y * q.z + q.w * q.x );
            float vz = 1 - 2 * ( q.x * q.x + q.y * q.y );
            float gain = still ? GRAVITY_GAIN_STILL : GRAVITY_GAIN_MOVING;
            rate[ 0 ] += gain * ( uy * vz - uz * vy );
            rate[ 1 ] += gain * ( uz * vx - ux * vz );
            rate[ 2 ] += gain * ( ux * vy - uy * vx );
        }

        quat_t dq = quat_mul( q, ( quat_t ){ 0, rate[ 0 ], rate[ 1 ], rate[ 2 ] } );
        q.w += 0.5f * dq.w * dt;
        q.x += 0.5f * dq.x * dt;
        q.y += 0.5f * dq.y * dt;
        q.z += 0.5f * dq.z * dt;
        quat_normalize( &q );

        if ( ++redraw_count >= REDRAW_EVERY_SAMPLES )
        {
            redraw_count = 0;
            /* IMU axes match the screen (X right, Y up, Z out), so no remap is needed. */
            float rotation[ 3 ][ 3 ];
            quat_to_matrix( quat_mul( q_ref_inv, q ), rotation );
            /* Short wait so a busy renderer drops a frame rather than stalling the filter. */
            if ( lvgl_port_lock( 10 ) )
            {
                cube_matrix_set( rotation );
                lv_obj_invalidate( cube_obj );
                lvgl_port_unlock();
            }
        }

        xTaskDelayUntil( &last_wake, pdMS_TO_TICKS( SAMPLE_PERIOD_MS ) );
    }
}
