/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * mpu.h
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

/**
 * @file mpu.h
 * @brief Inertial Measurement Unit (IMU) 3D orientation cube tab interface.
 */

#pragma once

#include <stdbool.h>
#include "lvgl.h"

/** @brief Tab identifier and log tag for the IMU tab. */
#define MPU_TAB_NAME "IMU"

/**
 * @brief Build the IMU 3D cube tab UI and start the sensor fusion task.
 *
 * @param tv Parent LVGL tabview widget.
 */
void display_mpu_tab( lv_obj_t *tv );

/**
 * @brief Wake or pause IMU sampling when entering or leaving the IMU tab.
 *
 * @param active true when the IMU tab is active; false when hidden.
 */
void mpu_set_active( bool active );

/**
 * @brief FreeRTOS task that fuses accelerometer and gyroscope readings into a 3D pose.
 *
 * @param pvParameters Unused task parameter.
 */
void MPU_task( void *pvParameters );
