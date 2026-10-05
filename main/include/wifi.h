/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * wifi.h
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
 * @file wifi.h
 * @brief ESP32 2.4 GHz Wi-Fi scanner, interactive connection UI, and status icon.
 */

#pragma once

#include <stdbool.h>
#include "lvgl.h"

/** @brief Tab identifier for the Wi-Fi tab. */
#define WIFI_TAB_NAME "ESP32-D0WD-WI-FI"

/**
 * @brief Build the Wi-Fi scanner/connection tab UI and start the Wi-Fi worker task.
 *
 * @param tv Parent LVGL tabview widget.
 */
void display_wifi_tab( lv_obj_t *tv );

/**
 * @brief Enable or pause periodic Wi-Fi scanning when entering or leaving the tab.
 *
 * Also dismisses any open password or error overlays when leaving the tab.
 *
 * @param active true when the Wi-Fi tab is active; false when hidden.
 */
void wifi_set_active( bool active );

/**
 * @brief Create the top-bar Wi-Fi connection status icon.
 *
 * Call with the LVGL port lock held.
 *
 * @param parent Parent container in the top status bar.
 * @return       Created Wi-Fi icon label widget.
 */
lv_obj_t *wifi_status_icon_create( lv_obj_t *parent );
