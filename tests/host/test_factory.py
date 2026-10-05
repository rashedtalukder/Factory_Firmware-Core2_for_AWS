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
#define pdTRUE 1
#define pdMS_TO_TICKS(value) (value)
#define ESP_LOGW(...) ((void)0)
typedef struct {uint8_t bands[2];} mic_frame_t;
static atomic_bool microphone_active = true;
static unsigned int disables, destroyed;
static jmp_buf finished;
static esp_err_t dsps_fft2r_init_fc32(float *table, int size) {return ESP_OK;}
static void dsps_fft2r_deinit_fc32(void) {destroyed++;}
static esp_err_t dsps_fft2r_fc32(float *data, int size) {assert(false); return ESP_OK;}
static esp_err_t dsps_bit_rev_fc32(float *data, int size) {assert(false); return ESP_OK;}
static esp_err_t dsps_wind_hann_f32(float *window, int size) {return ESP_OK;}
static esp_err_t core2foraws_audio_mic_enable(bool enable) {
    if (enable) {atomic_store(&microphone_active, false); return 9;}
    return ++disables == 1 ? 9 : ESP_OK;
}
static esp_err_t core2foraws_audio_mic_read(int8_t *data, size_t size, size_t *read) {assert(false); return 9;}
static void microphone_listening_set(bool listening) {}
static void spectrum_to_bands(const float *data, mic_frame_t *frame) {assert(false);}
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

    def test_wifi_list_keeps_strongest_record_per_ssid(self):
        source = (Path(__file__).resolve().parents[2] / "main/wifi.c").read_text()
        unique = source[source.index("static bool wifi_record_before("):source.index("static void add_ap_item(")]
        program = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define WIFI_RSSI_BAND_DB 6
typedef struct { uint8_t ssid[33]; int8_t rssi; } wifi_ap_record_t;
""" + unique + r"""
static wifi_ap_record_t ap(const char *ssid, int8_t rssi) {
    wifi_ap_record_t record = {{0}, rssi}; strcpy((char *)record.ssid, ssid); return record;
}
int main(void) {
    wifi_ap_record_t records[] = {
        ap("cafe", -80), ap("home", -70), ap("", -20), ap("cafe", -40),
        ap("home", -75), ap("lab", -60), ap("cafe", -90), ap("", -30),
    };
    uint16_t count = wifi_unique_networks(records, 8);
    assert(count == 3);
    assert(strcmp((char *)records[0].ssid, "cafe") == 0 && records[0].rssi == -40);
    assert(strcmp((char *)records[1].ssid, "lab") == 0 && records[1].rssi == -60);
    assert(strcmp((char *)records[2].ssid, "home") == 0 && records[2].rssi == -70);
    assert(wifi_unique_networks(records, 0) == 0);
    wifi_ap_record_t hidden[] = { ap("", -10) };
    assert(wifi_unique_networks(hidden, 1) == 0);
    wifi_ap_record_t close[] = { ap("zeta", -33), ap("alpha", -34), ap("mid", -50) };
    assert(wifi_unique_networks(close, 3) == 3);
    assert(strcmp((char *)close[0].ssid, "alpha") == 0 && strcmp((char *)close[1].ssid, "zeta") == 0);
    assert(strcmp((char *)close[2].ssid, "mid") == 0);
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "wifi")
            subprocess.run(["cc", "-x", "c", "-std=c11", "-Wall", "-Werror", "-fsanitize=address,undefined",
                            "-o", executable, "-"],
                           input=program.encode(), check=True, capture_output=True)
            subprocess.run([executable], check=True, capture_output=True)

    def test_battery_worker_publishes_state_without_widgets(self):
        source = (Path(__file__).resolve().parents[2] / "main/power.c").read_text()
        worker = source[source.index("void battery_task("):]
        program = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
typedef int esp_err_t;
typedef struct {} lv_subject_t;
#define ESP_OK 0
#define ESP_LOGW(...) ((void)0)
#define pdMS_TO_TICKS(value) (value)
static lv_subject_t subject;
static lv_subject_t *battery_state = &subject;
static const struct { float min_volts; } battery_levels[] = {
    {4.10f}, {3.95f}, {3.80f}, {3.25f}, {0.00f}
};
static float voltages[] = {4.2f, 3.9f, 3.1f, 3.1f};
static bool charging[] = {false, true, true, true};
static int reads, updates, published[4];
static jmp_buf finished;
static esp_err_t core2foraws_power_batt_volts_get(float *voltage) {
    *voltage = voltages[reads]; return ESP_OK;
}
static esp_err_t core2foraws_power_charging_get(bool *value) {
    *value = charging[reads++]; return ESP_OK;
}
static bool lvgl_port_lock(int timeout) {assert(timeout == 1000); return true;}
static void lvgl_port_unlock(void) {}
static void lv_subject_set_int(lv_subject_t *value, int state) {
    assert(value == &subject); published[updates++] = state;
}
static void vTaskDelay(int ticks) {
    assert(ticks == 1000);
    if (reads == 4) longjmp(finished, 1);
}
""" + worker + r"""
int main(void) {
    if (setjmp(finished) == 0) battery_task(NULL);
    assert(updates == 3);
    assert(published[0] == 0 && published[1] == 10 && published[2] == 12);
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as directory:
            executable = str(Path(directory) / "battery")
            subprocess.run(["cc", "-x", "c", "-std=c11", "-Wall", "-Werror", "-fsanitize=address,undefined",
                            "-o", executable, "-"], input=program.encode(), check=True, capture_output=True)
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