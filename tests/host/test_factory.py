from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


@unittest.skipUnless(shutil.which("cc"), "native C compiler required")
class FactoryWorkerTests(unittest.TestCase):
    def test_sound_task_retains_failed_cleanup(self):
        source = (Path(__file__).resolve().parents[2] / "main/sound.c").read_text()
        worker = source[source.index("void sound_task("):]
        program = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define pdMS_TO_TICKS(value) (value)
const unsigned char music[120264] = {0};
static bool enable_fails;
static unsigned int disables, drains, deleted;
static esp_err_t core2foraws_audio_speaker_enable(bool enable) {
    if (enable) return enable_fails ? 9 : ESP_OK;
    return ++disables % 2 ? 9 : ESP_OK;
}
static esp_err_t core2foraws_audio_speaker_write(const uint8_t *data, size_t length) {assert(!enable_fails); return ESP_OK;}
static esp_err_t core2foraws_audio_speaker_drain(void) {drains++; return ESP_OK;}
static void vTaskDelay(unsigned int ticks) {assert(ticks == 500);}
static void vTaskDelete(void *task) {assert(disables % 2 == 0); deleted++;}
""" + worker + r"""
int main(void) {
    enable_fails = true; sound_task(NULL);
    assert(disables == 2 && drains == 0 && deleted == 1);
    enable_fails = false; sound_task(NULL);
    assert(disables == 4 && drains == 1 && deleted == 2);
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "sound")
            subprocess.run(["cc", "-x", "c", "-std=c11", "-fsanitize=address,undefined", "-o", executable, "-"],
                           input=program.encode(), check=True, capture_output=True)
            subprocess.run([executable], check=True, capture_output=True)

    def test_microphone_failed_start_cleanup_survives_tab_exit(self):
        source = (Path(__file__).resolve().parents[2] / "main/mic.c").read_text()
        worker = source[source.index("void microphoneTask("):source.index("void fft_show_task(")]
        program = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <stdatomic.h>
#include <setjmp.h>
#include <math.h>
typedef int esp_err_t;
typedef void *QueueHandle_t;
#define ESP_OK 0
#define FFT_SIZE 4
#define FFT_REAL 0
#define FFT_FORWARD 0
#define CANVAS_HEIGHT 2
#define pdTRUE 1
#define pdMS_TO_TICKS(value) (value)
#define ESP_LOGW(...) ((void)0)
typedef struct {uint8_t spectrum[CANVAS_HEIGHT];} mic_frame_t;
typedef struct {unsigned int size; float input[4]; float output[8];} fft_config_t;
static fft_config_t plan = {.size = 4};
static atomic_bool microphone_active = true;
static unsigned int disables, destroyed;
static jmp_buf finished;
static fft_config_t *fft_init(int size, int type, int direction, void *input, void *output) {return &plan;}
static void fft_destroy(fft_config_t *config) {destroyed++;}
static void fft_execute(fft_config_t *config) {assert(false);}
static esp_err_t core2foraws_audio_mic_enable(bool enable) {
    if (enable) {atomic_store(&microphone_active, false); return 9;}
    return ++disables == 1 ? 9 : ESP_OK;
}
static esp_err_t core2foraws_audio_mic_read(int8_t *data, size_t size, size_t *read) {assert(false); return 9;}
static void microphone_status_set(const char *text) {}
static long map_long(long value, long low, long high, long out_low, long out_high) {return 0;}
static uint8_t clamp_u8(int value) {return 0;}
static void xQueueOverwrite(void *queue, const void *frame) {assert(false);}
static void vTaskDelay(unsigned int ticks) {}
static void ulTaskNotifyTake(int clear, unsigned int ticks) {
    assert(disables == 2 && destroyed == 1); longjmp(finished, 1);
}
""" + worker + r"""
int main(void) {
    if (setjmp(finished) == 0) microphoneTask(NULL);
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "microphone")
            subprocess.run(["cc", "-x", "c", "-std=c11", "-fsanitize=address,undefined", "-o", executable, "-"],
                           input=program.encode(), check=True, capture_output=True)
            subprocess.run([executable], check=True, capture_output=True)

    def test_bsp_scan_ownership_and_retry(self):
        source = (Path(__file__).resolve().parents[2] /
                  "components/Core2-for-AWS-IoT-Kit/lib/wifi/core2foraws_wifi.c").read_text()
        scan = source[source.index("esp_err_t core2foraws_wifi_scan("):source.index("static esp_err_t _core2foraws_wifi_deinit_locked( void )\n{")]
        program = r"""
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
typedef int esp_err_t;
typedef int wifi_ap_record_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_INVALID_STATE 2
#define WIFI_MODE_STA 1
static bool _wifi_initialized, held;
static atomic_bool _wifi_started, _scan_only, _provisioning_active;
static unsigned int starts, clears;
static esp_err_t scan_error, mode_error;
static esp_err_t _wifi_lifecycle_lock(void) {assert(!held); held = true; return ESP_OK;}
static void _wifi_lifecycle_unlock(void) {assert(held); held = false;}
static esp_err_t esp_wifi_set_mode(int mode) {assert(held && mode == WIFI_MODE_STA); return mode_error;}
static esp_err_t esp_wifi_start(void) {assert(held && atomic_load(&_scan_only)); starts++; return ESP_OK;}
static esp_err_t esp_wifi_scan_start(void *config, bool block) {assert(held && block); return scan_error;}
static esp_err_t esp_wifi_scan_get_ap_records(uint16_t *count, wifi_ap_record_t *records) {assert(held); *count = 1; records[0] = 42; return ESP_OK;}
static void esp_wifi_clear_ap_list(void) {assert(held); clears++;}
""" + scan + r"""
int main(void) {
    wifi_ap_record_t records[2]; uint16_t count = 2;
    assert(core2foraws_wifi_scan(NULL, &count) == ESP_ERR_INVALID_ARG);
    assert(core2foraws_wifi_scan(records, &count) == ESP_ERR_INVALID_STATE && !held);
    _wifi_initialized = true; mode_error = 7;
    assert(core2foraws_wifi_scan(records, &count) == 7 && starts == 0 && !held);
    mode_error = 0;
    assert(core2foraws_wifi_scan(records, &count) == ESP_OK && starts == 1 && records[0] == 42);
    assert(core2foraws_wifi_scan(records, &count) == ESP_OK && starts == 1);
    scan_error = 9;
    assert(core2foraws_wifi_scan(records, &count) == 9 && clears == 1 && !held);
    atomic_store(&_provisioning_active, true);
    assert(core2foraws_wifi_scan(records, &count) == ESP_ERR_INVALID_STATE && !held);
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "wifi")
            subprocess.run(["cc", "-x", "c", "-std=c11", "-fsanitize=address,undefined", "-o", executable, "-"],
                           input=program.encode(), check=True, capture_output=True)
            subprocess.run([executable], check=True, capture_output=True)

    def test_led_reentry_and_failed_commit(self):
        source = (Path(__file__).resolve().parents[2] / "main/led_bar.c").read_text()
        solid = source[source.index("static void show_solid_until_inactive"):source.index("void sk6812_animation_task(")]
        program = r"""
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
typedef int esp_err_t;
#define ESP_OK 0
#define RGB_LED_SIDE_LEFT 0
#define RGB_LED_SIDE_RIGHT 1
#define pdTRUE 1
#define pdMS_TO_TICKS(value) (value)
#define ESP_LOGD(...) ((void)0)
static atomic_bool solid_active;
static unsigned int writes, loops, failures, brightness;
static uint32_t color;
static bool color_snapshot(uint8_t *red, uint8_t *green, uint8_t *blue) {
    *red = 0x12; *green = 0x34; *blue = 0x56; return true;
}
static int core2foraws_rgb_led_side_color_set(int side, uint32_t value) {color = value; writes++; return ESP_OK;}
static int core2foraws_rgb_led_brightness_set(int value) {brightness = value; return ESP_OK;}
static bool led_commit(esp_err_t result, const char *operation) {
    if (failures > 0) {failures--; return false;} return true;
}
static void ulTaskNotifyTake(int clear, int timeout) {
    if (++loops >= 2) atomic_store(&solid_active, false);
}
""" + solid + r"""
int main(void) {
    atomic_store(&solid_active, true); show_solid_until_inactive();
    assert(writes == 2 && color == 0x123456 && brightness == 100);
    color = 0; brightness = 20; loops = 0;
    atomic_store(&solid_active, true); show_solid_until_inactive();
    assert(writes == 4 && color == 0x123456 && brightness == 100);
    failures = 1; loops = 0;
    atomic_store(&solid_active, true); show_solid_until_inactive();
    assert(writes == 8 && failures == 0);
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "led")
            subprocess.run(["cc", "-x", "c", "-std=c11", "-fsanitize=address,undefined", "-o", executable, "-"],
                           input=program.encode(), check=True, capture_output=True)
            subprocess.run([executable], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()