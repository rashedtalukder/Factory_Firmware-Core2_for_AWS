/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * led_bar.h
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
 * @file led_bar.h
 * @brief SK6812 RGB LED bar tab interface and animation task.
 */

#pragma once

#include <stdbool.h>
#include "lvgl.h"

/** @brief Tab identifier and log tag for the SK6812 LED bar tab. */
#define LED_BAR_TAB_NAME "SK6812-LED_BAR"

/**
 * @brief Build the SK6812 RGB LED bar tab UI and start the LED task.
 *
 * @param tv Parent LVGL tabview widget.
 */
void display_LED_bar_tab( lv_obj_t *tv );

/**
 * @brief Switch the LED bars between solid slider-controlled color (when active)
 *        and the idle chase animation (when inactive).
 *
 * @param active true when the LEDs tab is active; false otherwise.
 */
void led_bar_set_active( bool active );

/**
 * @brief FreeRTOS task driving the SK6812 idle animation and solid color updates.
 *
 * @param pvParameters Unused task parameter.
 */
void sk6812_animation_task( void *pvParameters );

