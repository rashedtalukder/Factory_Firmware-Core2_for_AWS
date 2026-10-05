/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * mic.c
 */

#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "mic.h"
#include "esp_dsp.h"

TaskHandle_t mic_handle, FFT_handle;
static atomic_bool microphone_active;
static lv_obj_t *viz_panel;

/* CHECKED mirrors whether the mic is capturing, so UI tests can observe it via mic.state. */
static void microphone_listening_set(bool listening)
{
    if (lvgl_port_lock(1000)) {
        lv_obj_set_state(viz_panel, LV_STATE_CHECKED, listening);
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

#define CANVAS_WIDTH 256
#define CANVAS_HEIGHT 110

#define FFT_SIZE 512
#define MIC_QUEUE_LEN 1

#define MIC_BANDS        24
#define MIC_BAR_WIDTH    8
#define MIC_BAR_GAP      2
#define MIC_BARS_X       ((CANVAS_WIDTH - (MIC_BANDS * (MIC_BAR_WIDTH + MIC_BAR_GAP) - MIC_BAR_GAP)) / 2)
#define MIC_SEG_PITCH    5
#define MIC_SEG_HEIGHT   4
#define MIC_SEGMENTS     (CANVAS_HEIGHT / MIC_SEG_PITCH)
/* 44.1 kHz / 512-point FFT = ~86 Hz per bin; bands span ~170 Hz to ~11 kHz. */
#define MIC_BAND_MIN_BIN 2
#define MIC_BAND_MAX_BIN 128
/* Band level in tenths of a dB of FFT magnitude; the PDM mic's own noise already rises with frequency. */
#define MIC_LEVEL_FLOOR  360
#define MIC_LEVEL_CEIL   760
#define MIC_DECAY_PX     4
#define MIC_PEAK_HOLD    18

typedef struct
{
    uint8_t bands[MIC_BANDS]; /* 0..CANVAS_HEIGHT */
} mic_frame_t;

static void spectrum_to_bands(const float *fft_data, mic_frame_t *frame)
{
    static uint16_t edges[MIC_BANDS + 1];
    if (edges[MIC_BANDS] == 0) {
        edges[0] = MIC_BAND_MIN_BIN;
        for (int b = 1; b <= MIC_BANDS; b++) {
            uint16_t edge = (uint16_t)lroundf(MIC_BAND_MIN_BIN *
                powf((float)MIC_BAND_MAX_BIN / MIC_BAND_MIN_BIN, (float)b / MIC_BANDS));
            edges[b] = edge > edges[b - 1] ? edge : edges[b - 1] + 1;
        }
    }

    for (int b = 0; b < MIC_BANDS; b++) {
        float peak = 0.0f;
        for (uint16_t bin = edges[b]; bin < edges[b + 1]; bin++) {
            float real = fft_data[2 * bin], imag = fft_data[2 * bin + 1];
            peak = fmaxf(peak, real * real + imag * imag);
        }
        /* 100*log10(power) == 200*log10(magnitude) */
        long level = (long)(100.0f * log10f(peak + 1.0f)) - MIC_LEVEL_FLOOR;
        long height = level * CANVAS_HEIGHT / (MIC_LEVEL_CEIL - MIC_LEVEL_FLOOR);
        frame->bands[b] = height < 0 ? 0 : height > CANVAS_HEIGHT ? CANVAS_HEIGHT : (uint8_t)height;
    }
}

void display_microphone_tab(lv_obj_t *tv)
{
    lvgl_port_lock(0);

    lv_obj_t *mic_tab = ui_tabview_add_tab(tv, MICROPHONE_TAB_NAME);

    lv_obj_t *card = ui_create_card(mic_tab, lv_color_make(0,0,0));

    static char title[32];
    core2foraws_board_info_t board;
    if (core2foraws_board_info_get(&board) == ESP_OK &&
        board.microphone != BOARD_MIC_UNKNOWN)
        snprintf(title, sizeof(title), "%s Microphone", board.microphone_name);
    else
        snprintf(title, sizeof(title), "Microphone");

    ui_card_title(card, title,
                  lv_palette_main(LV_PALETTE_LIME));

    /* spectrum container: fills the card below the title */
    viz_panel = lv_obj_create(card);
    ui_test_id(viz_panel, "mic.state");

    lv_obj_set_size(viz_panel, CANVAS_WIDTH + 6, CANVAS_HEIGHT + 6);

    lv_obj_set_scrollable(viz_panel, false);

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

    bool fft_ready = false;

    mic_frame_t frame;

    /* Interleaved re/im pairs for the complex FFT. */
    static float fft_data[FFT_SIZE * 2] __attribute__((aligned(16)));
    static float window[FFT_SIZE] __attribute__((aligned(16)));
    dsps_wind_hann_f32(window, FFT_SIZE);
    /* x2 offsets the Hann window's 0.5 coherent gain. */
    const float scale = 2.0f * 1000.0f / 32768.0f;

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
            microphone_listening_set(false);
        }
        if (!atomic_load(&microphone_active)) {
            if (owns_microphone) {
                err = core2foraws_audio_mic_enable(false);
                if (err != ESP_OK) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    continue;
                }
                owns_microphone = false;
                microphone_listening_set(false);
            }
            if (fft_ready) {
                dsps_fft2r_deinit_fc32();
                fft_ready = false;
            }
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
            continue;
        }
        if (!fft_ready) {
            if (dsps_fft2r_init_fc32(NULL, FFT_SIZE) != ESP_OK) {
                ESP_LOGW(TAG, "FFT allocation failed; retrying");
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            fft_ready = true;
        }
        if (!owns_microphone) {
            err = core2foraws_audio_mic_enable(true);
            if (err != ESP_OK) {
                cleanup_pending = true;
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            owns_microphone = true;
            microphone_listening_set(true);
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

        for(uint16_t i=0;i<FFT_SIZE;i++)
        {
            fft_data[2*i] = (float)mic_samples[i] * scale * window[i];
            fft_data[2*i+1] = 0.0f;
        }

        dsps_fft2r_fc32(fft_data, FFT_SIZE);
        dsps_bit_rev_fc32(fft_data, FFT_SIZE);

        spectrum_to_bands(fft_data, &frame);

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

    mic_frame_t frame;

    memset(&frame,0,sizeof(frame));

    lvgl_port_lock(0);

    lv_obj_t *canvas =
        lv_canvas_create((lv_obj_t*)pvParameters);

    void *cbuf =
        heap_caps_malloc(
            LV_CANVAS_BUF_SIZE(CANVAS_WIDTH, CANVAS_HEIGHT, 16,
                               LV_DRAW_BUF_STRIDE_ALIGN),
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
        LV_COLOR_FORMAT_RGB565
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

    /* Left-to-right gradient; each column gets a lit, dim (unlit segment) and peak-cap shade. */
    static const uint32_t stops[] = { 0x00e5ff, 0x7c4dff, 0xff4fa3, 0xff9900 };
    const int spans = sizeof(stops) / sizeof(stops[0]) - 1;
    static lv_color16_t lit[CANVAS_WIDTH], dim[CANVAS_WIDTH], cap[CANVAS_WIDTH];
    for (int x = 0; x < CANVAS_WIDTH; x++) {
        int pos = x * spans * 255 / (CANVAS_WIDTH - 1);
        int span = pos / 255 < spans ? pos / 255 : spans - 1;
        lv_color_t c = lv_color_mix(lv_color_hex(stops[span + 1]), lv_color_hex(stops[span]),
                                    (uint8_t)(pos - span * 255));
        lv_color_t shades[] = { c, lv_color_mix(c, lv_color_black(), 56), lv_color_mix(lv_color_white(), c, 170) };
        lv_color16_t *out[] = { &lit[x], &dim[x], &cap[x] };
        for (int i = 0; i < 3; i++) {
            out[i]->red = shades[i].red >> 3;
            out[i]->green = shades[i].green >> 2;
            out[i]->blue = shades[i].blue >> 3;
        }
    }
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(canvas);
    uint8_t heights[MIC_BANDS] = {0}, peaks[MIC_BANDS] = {0}, holds[MIC_BANDS] = {0};

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
            for (int b = 0; b < MIC_BANDS; b++) {
                /* Fast attack, gravity-style decay, and a peak cap that holds before falling. */
                if (frame.bands[b] >= heights[b]) heights[b] = frame.bands[b];
                else heights[b] = heights[b] > frame.bands[b] + MIC_DECAY_PX ? heights[b] - MIC_DECAY_PX : frame.bands[b];
                if (heights[b] >= peaks[b]) { peaks[b] = heights[b]; holds[b] = MIC_PEAK_HOLD; }
                else if (holds[b]) holds[b]--;
                else peaks[b]--;
            }

            /* Bounded wait; drop this frame if the LVGL render loop is busy. */
            if(!lvgl_port_lock(1000))
            {
                ESP_LOGW(TAG,"LVGL lock timeout; skipping spectrum frame");
                continue;
            }

            for (int y = 0; y < CANVAS_HEIGHT; y++) {
                lv_color16_t *row = lv_draw_buf_goto_xy(draw_buf, 0, y);
                int from_bottom = CANVAS_HEIGHT - 1 - y;
                int segment = from_bottom / MIC_SEG_PITCH;
                bool in_segment = from_bottom % MIC_SEG_PITCH < MIC_SEG_HEIGHT && segment < MIC_SEGMENTS;
                for (int b = 0; b < MIC_BANDS; b++) {
                    int lit_segments = (heights[b] + MIC_SEG_PITCH / 2) / MIC_SEG_PITCH;
                    int peak_segment = peaks[b] / MIC_SEG_PITCH;
                    int x0 = MIC_BARS_X + b * (MIC_BAR_WIDTH + MIC_BAR_GAP);
                    for (int x = x0; x < x0 + MIC_BAR_WIDTH; x++) {
                        if (!in_segment) row[x] = (lv_color16_t){0};
                        else if (segment < lit_segments) row[x] = lit[x];
                        else if (peaks[b] && segment == peak_segment) row[x] = cap[x];
                        else row[x] = dim[x];
                    }
                }
            }
            lv_obj_invalidate(canvas);

            lvgl_port_unlock();
        }
    }
}