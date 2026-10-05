/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * clock.h
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
 * @file clock.h
 * @brief BM8563 real-time clock tab interface and background refresh task.
 */

#pragma once

#include <stdbool.h>
#include "lvgl.h"

/** @brief Tab identifier and log tag for the BM8563 RTC tab. */
#define CLOCK_TAB_NAME "BM8563-CLOCK"

/**
 * @brief Build the BM8563 real-time clock tab UI and start the RTC worker task.
 *
 * @param tv Parent LVGL tabview widget.
 */
void display_clock_tab( lv_obj_t *tv );

/**
 * @brief Notify the clock tab when it becomes visible or hidden.
 *
 * @param active true when the tab is active; false when hidden.
 */
void clock_set_active( bool active );

/**
 * @brief Apply the currently selected hour and minute roller values to the RTC.
 *
 * Invoked when the right capacitive touch button is pressed on the Clock tab.
 */
void clock_on_right_press( void );

/**
 * @brief FreeRTOS task that periodically reads and updates the BM8563 RTC time.
 *
 * @param pvParameters Unused task parameter.
 */
void clock_task( void *pvParameters );