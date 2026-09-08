/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * sound.c
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "core2foraws.h"

#include "sound.h"

static const char *TAG = "SOUND";

void sound_task( void *pvParameters )
{
    ESP_LOGD( TAG, "Playing startup sound" );
    esp_err_t err = core2foraws_audio_speaker_enable( true );
    if ( err == ESP_OK )
    {    
        extern const unsigned char music[ 120264 ];
        err = core2foraws_audio_speaker_write( ( const uint8_t * )music, 120264 );
        if (err == ESP_OK) err = core2foraws_audio_speaker_drain();
        if ( err != ESP_OK )
            ESP_LOGE( TAG, "Failed to play startup sound: %s", esp_err_to_name( err ) );

    }
    else
    {
        ESP_LOGE( TAG, "Failed to enable speaker: %s", esp_err_to_name( err ) );
    }

    esp_err_t disable_err;
    while ((disable_err = core2foraws_audio_speaker_enable(false)) != ESP_OK) {
        ESP_LOGW(TAG, "Speaker cleanup pending: %s", esp_err_to_name(disable_err));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (err == ESP_OK) ESP_LOGD(TAG, "Startup sound finished");

    vTaskDelete( NULL ); // Deletes the current task from FreeRTOS task list and the FreeRTOS idle task will remove from memory.
}