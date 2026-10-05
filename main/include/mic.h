/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * mic.h
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
 * @file mic.h
 * @brief PDM microphone real-time FFT spectrum analyzer tab interface.
 */

#pragma once

#include <stdbool.h>
#include "lvgl.h"

/** @brief Tab identifier and log tag for the Microphone tab. */
#define MICROPHONE_TAB_NAME "PDM-MIC"

/**
 * @brief Build the Microphone FFT spectrum analyzer tab UI and start its worker task.
 *
 * @param tv Parent LVGL tabview widget.
 */
void display_microphone_tab( lv_obj_t *tv );

/**
 * @brief Enable or disable microphone capture and FFT rendering when switching tabs.
 *
 * Releases the I2S bus and SPIRAM FFT buffers when inactive so the speaker and
 * other peripherals can run unimpeded.
 *
 * @param active true when the Mic tab is active; false when hidden.
 */
void microphone_set_active( bool active );

/**
 * @brief FreeRTOS task that samples the PDM microphone and computes 24-band FFT levels.
 *
 * @param pvParameters Queue handle receiving computed mic_frame_t spectrum frames.
 */
void microphoneTask( void *pvParameters );

/**
 * @brief FreeRTOS task that renders FFT spectrum bars onto the LVGL canvas.
 *
 * @param pvParameters Parent LVGL container widget for the spectrum canvas.
 */
void fft_show_task( void *pvParameters );
