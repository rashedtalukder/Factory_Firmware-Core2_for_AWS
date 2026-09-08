/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Factory Firmware v2.3.0
 * mic.c
 */

#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "mic.h"
#include "fft.h"

TaskHandle_t mic_handle, FFT_handle;
static atomic_bool microphone_active;
static lv_obj_t *microphone_status;

static void microphone_status_set(const char *text)
{
    if (lvgl_port_lock(1000)) {
        lv_label_set_text(microphone_status, text);
        lvgl_port_unlock();
    }
}

void microphone_set_active(bool active)
{
    atomic_store(&microphone_active, active);
    if (mic_handle) xTaskNotifyGive(mic_handle);
    if (FFT_handle) xTaskNotifyGive(FFT_handle);
}

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

    microphone_status = ui_card_text(card, "Idle", lv_color_make(255,255,255));
    ui_test_id(microphone_status, "mic.state");

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

    ESP_LOGD(TAG,"Building tab");

    if(xTaskCreatePinnedToCore(
        fft_show_task,
        "fftShowTask",
        4096*2,
        (void*)viz_panel,
        1,
        &FFT_handle,
        1
    )!=pdPASS)
    {
        FFT_handle=NULL;
        ESP_LOGE(TAG,"Failed to create FFT display task");
    }
}

void microphoneTask(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;

    static int16_t mic_samples[FFT_SIZE];

    size_t bytesread=0;

    esp_err_t err;
    bool owns_microphone = false;
    bool cleanup_pending = false;

    fft_config_t *fft_plan = NULL;

    mic_frame_t frame;

    for(;;)
    {
        if (cleanup_pending) {
            err = core2foraws_audio_mic_enable(false);
            if (err != ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            cleanup_pending = false;
            owns_microphone = false;
            microphone_status_set("Idle");
        }
        if (!atomic_load(&microphone_active)) {
            if (owns_microphone) {
                err = core2foraws_audio_mic_enable(false);
                if (err != ESP_OK) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    continue;
                }
                owns_microphone = false;
                microphone_status_set("Idle");
            }
            if (fft_plan != NULL) {
                fft_destroy(fft_plan);
                fft_plan = NULL;
            }
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
            continue;
        }
        if (fft_plan == NULL) {
            fft_plan = fft_init(FFT_SIZE,FFT_REAL,FFT_FORWARD,NULL,NULL);
            if (fft_plan == NULL) {
                ESP_LOGW(TAG, "FFT allocation failed; retrying");
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
        }
        if (!owns_microphone) {
            err = core2foraws_audio_mic_enable(true);
            if (err != ESP_OK) {
                cleanup_pending = true;
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            owns_microphone = true;
            microphone_status_set("Listening");
        }
        memset(&frame,0,sizeof(frame));

        err=core2foraws_audio_mic_read(
            (int8_t*)mic_samples,
            sizeof(mic_samples),
            &bytesread
        );
        if(err!=ESP_OK || bytesread!=sizeof(mic_samples))
        {
            if(err!=ESP_OK)
                ESP_LOGW(TAG,"Microphone read failed: %s",esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        for(uint16_t i=0;i<fft_plan->size;i++)
        {
            fft_plan->input[i] =
                (float)map_long(
                    mic_samples[i],
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
retry:
    while (!atomic_load(&microphone_active))
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    QueueHandle_t mic_queue =
        xQueueCreate(MIC_QUEUE_LEN,sizeof(mic_frame_t));

    if(mic_queue==NULL)
    {
        ESP_LOGE(TAG,"Failed to create mic queue");
        vTaskDelay(pdMS_TO_TICKS(500));
        goto retry;
    }

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

    if(cbuf==NULL || canvas == NULL)
    {
        if (canvas != NULL) lv_obj_delete(canvas);
        free(cbuf);
        lvgl_port_unlock();
        ESP_LOGE(TAG,"Canvas alloc failed");
        vQueueDelete(mic_queue);
        vTaskDelay(pdMS_TO_TICKS(500));
        goto retry;
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

    if(xTaskCreatePinnedToCore(
        microphoneTask,
        "microphoneTask",
        4096*2,
        (void*)mic_queue,
        1,
        &mic_handle,
        1
    )!=pdPASS)
    {
        mic_handle=NULL;
        ESP_LOGE(TAG,"Failed to create microphone task");
        lvgl_port_lock(0);
        lv_obj_delete(canvas);
        lvgl_port_unlock();
        free(cbuf);
        vQueueDelete(mic_queue);
        vTaskDelay(pdMS_TO_TICKS(500));
        goto retry;
    }

    extern const unsigned char color_map[768];

    for(;;)
    {
        if (!atomic_load(&microphone_active)) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
            continue;
        }
        if(xQueueReceive(
            mic_queue,
            &frame,
            pdMS_TO_TICKS(10)
        )==pdPASS)
        {
            /* Bounded wait with padding; drop this spectrum column if the
             * LVGL render loop is busy rather than blocking forever. */
            if(!lvgl_port_lock(1000))
            {
                ESP_LOGW(TAG,"LVGL lock timeout; skipping spectrum column");
                continue;
            }

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