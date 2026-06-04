/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * mic.c
 */

#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "mic.h"
#include "fft.h"

TaskHandle_t mic_handle, FFT_handle;

static const char *TAG = MICROPHONE_TAB_NAME;

#define CANVAS_WIDTH 248
#define CANVAS_HEIGHT 52

#define FFT_SIZE 512
#define MIC_QUEUE_LEN 1

typedef struct
{
    uint8_t spectrum[CANVAS_HEIGHT];
} mic_frame_t;

static long map_long(long x,long in_min,long in_max,long out_min,long out_max)
{
    long divisor = (in_max - in_min);

    if(divisor == 0)
        return out_min;

    return (x-in_min)*(out_max-out_min)/divisor + out_min;
}

static inline uint8_t clamp_u8(int value)
{
    if(value < 0) return 0;
    if(value > 255) return 255;
    return (uint8_t)value;
}

void display_microphone_tab(lv_obj_t *tv)
{
    lvgl_port_lock(0);

    lv_obj_t *mic_tab = ui_tabview_add_tab(tv, MICROPHONE_TAB_NAME);

    lv_obj_t *card = ui_create_card(mic_tab, lv_color_make(0,0,0));

    ui_card_title(card,"SPM1423 Microphone",
                  lv_palette_main(LV_PALETTE_LIME));

    ui_card_text(card,
        "The SPM1423 is an enhanced far-field MEMS microphone.\n\n"
        "Say \"Hi EduKit\"",
        lv_color_make(255,255,255));

    /* spectrum container */
    lv_obj_t *viz_panel = lv_obj_create(card);

    lv_obj_set_size(viz_panel,252,58);

    lv_obj_clear_flag(viz_panel,LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(viz_panel,lv_color_hex(0x050505),0);
    lv_obj_set_style_bg_opa(viz_panel,LV_OPA_COVER,0);

    lv_obj_set_style_border_color(viz_panel,lv_color_hex(0x303030),0);
    lv_obj_set_style_border_width(viz_panel,1,0);

    lv_obj_set_style_radius(viz_panel,4,0);

    lv_obj_set_style_pad_all(viz_panel,2,0);

    lvgl_port_unlock();

    ESP_LOGI(TAG,"Displaying tab");

    xTaskCreatePinnedToCore(
        fft_show_task,
        "fftShowTask",
        4096*2,
        (void*)viz_panel,
        1,
        &FFT_handle,
        1
    );
}

void microphoneTask(void *pvParameters)
{
    vTaskSuspend(NULL);

    QueueHandle_t queue = (QueueHandle_t)pvParameters;

    static int8_t i2s_readraw_buff[1024];

    size_t bytesread=0;

    int16_t *buffptr=NULL;

    core2foraws_audio_mic_enable(true);

    fft_config_t *fft_plan =
        fft_init(FFT_SIZE,FFT_REAL,FFT_FORWARD,NULL,NULL);

    if(fft_plan==NULL)
    {
        ESP_LOGE(TAG,"fft_init failed");
        vTaskDelete(NULL);
        return;
    }

    mic_frame_t frame;

    for(;;)
    {
        memset(&frame,0,sizeof(frame));

        core2foraws_audio_mic_read(
            i2s_readraw_buff,
            sizeof(i2s_readraw_buff),
            &bytesread
        );

        buffptr=(int16_t*)i2s_readraw_buff;

        for(uint16_t i=0;i<fft_plan->size;i++)
        {
            fft_plan->input[i] =
                (float)map_long(
                    buffptr[i],
                    INT16_MIN,
                    INT16_MAX,
                    -1000,
                    1000
                );
        }

        fft_execute(fft_plan);

        for(uint16_t bin=1;bin<CANVAS_HEIGHT;bin++)
        {
            float real=fft_plan->output[2*bin];
            float imag=fft_plan->output[2*bin+1];

            float mag=sqrtf(real*real + imag*imag);

            int color_value =
                map_long((long)mag,0,2600,0,255);

            frame.spectrum[CANVAS_HEIGHT-bin] =
                clamp_u8(color_value);
        }

        xQueueOverwrite(queue,&frame);

        vTaskDelay(pdMS_TO_TICKS(12));
    }
}

void fft_show_task(void *pvParameters)
{
    QueueHandle_t mic_queue =
        xQueueCreate(MIC_QUEUE_LEN,sizeof(mic_frame_t));

    if(mic_queue==NULL)
    {
        ESP_LOGE(TAG,"Failed to create mic queue");
        vTaskDelete(NULL);
        return;
    }

    xTaskCreatePinnedToCore(
        microphoneTask,
        "microphoneTask",
        4096*2,
        (void*)mic_queue,
        1,
        &mic_handle,
        1
    );

    vTaskSuspend(NULL);

    static uint16_t position_data=0;

    mic_frame_t frame;

    memset(&frame,0,sizeof(frame));

    lvgl_port_lock(0);

    lv_obj_t *canvas =
        lv_canvas_create((lv_obj_t*)pvParameters);

    lv_color_t *cbuf =
        heap_caps_malloc(
            CANVAS_WIDTH*
            CANVAS_HEIGHT*
            sizeof(lv_color_t),
            MALLOC_CAP_DEFAULT|
            MALLOC_CAP_SPIRAM
        );

    if(cbuf==NULL)
    {
        lvgl_port_unlock();
        ESP_LOGE(TAG,"Canvas alloc failed");
        vTaskDelete(NULL);
        return;
    }

    lv_canvas_set_buffer(
        canvas,
        cbuf,
        CANVAS_WIDTH,
        CANVAS_HEIGHT,
        LV_COLOR_FORMAT_NATIVE
    );

    lv_canvas_fill_bg(
        canvas,
        lv_color_make(0,0,0),
        LV_OPA_COVER
    );

    lv_obj_align(canvas,LV_ALIGN_CENTER,0,0);

    lvgl_port_unlock();

    extern const unsigned char color_map[768];

    for(;;)
    {
        if(xQueueReceive(
            mic_queue,
            &frame,
            pdMS_TO_TICKS(10)
        )==pdPASS)
        {
            lvgl_port_lock(0);

            for(uint16_t y=0;y<CANVAS_HEIGHT;y++)
            {
                uint8_t color_position =
                    frame.spectrum[y];

                lv_color_t px =
                    lv_color_make(
                        color_map[color_position*3+0],
                        color_map[color_position*3+1],
                        color_map[color_position*3+2]
                    );

                lv_canvas_set_px(
                    canvas,
                    position_data,
                    y,
                    px,
                    LV_OPA_COVER
                );
            }

            lvgl_port_unlock();

            position_data++;

            if(position_data>=CANVAS_WIDTH)
                position_data=0;
        }
    }
}