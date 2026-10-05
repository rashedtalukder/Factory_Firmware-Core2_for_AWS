/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * power.h
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
 * @file power.h
 * @brief AXP192 power management tab and top-bar battery status indicator.
 */

#pragma once

#include "lvgl.h"

/** @brief Tab identifier and log tag for the AXP192 Power tab. */
#define POWER_TAB_NAME "AXP192-POWER"

/** @brief Fixed width in pixels of the top-bar battery indicator widget. */
#define BATTERY_INDICATOR_WIDTH 22

/**
 * @brief Create the top-bar battery level and charging indicator widget.
 *
 * @param parent Parent container in the top status bar.
 * @return       Created battery indicator container object.
 */
lv_obj_t *battery_indicator_create( lv_obj_t *parent );

/**
 * @brief Build the AXP192 Power Management tab UI and start the battery worker task.
 *
 * @param tv Parent LVGL tabview widget.
 */
void display_power_tab( lv_obj_t *tv );

/**
 * @brief FreeRTOS task that services queued AXP192 peripheral toggles and polls
 *        battery voltage and charging status.
 *
 * @param pvParameters Unused task parameter.
 */
void battery_task( void *pvParameters );