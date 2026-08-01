/*
 * SPDX-FileCopyrightText: 2026 Rashed Talukder
 * SPDX-License-Identifier: Apache-2.0
 *
 * Core2 for AWS UI Test Harness
 */

#include "sdkconfig.h"
#include "uitest.h"

#ifdef CONFIG_UITEST_ENABLED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_rom_crc.h"

#include "lvgl.h"
#include "esp_lvgl_port.h"

#ifdef CONFIG_SCREENSHOT_ENABLED
#include "screenshot.h"
#endif

static const char *TAG = "UITEST";

#define RESP_PREFIX  CONFIG_UITEST_CMD_PREFIX
#define READ_PERIOD  CONFIG_UITEST_READ_PERIOD_MS

/* ── Synthetic input device ─────────────────────────────────────── */

/* One injected pointer sample consumed per LVGL indev read. */
typedef struct {
    int16_t x;
    int16_t y;
    uint8_t pressed;    /* 1 = LV_INDEV_STATE_PRESSED, 0 = RELEASED */
} inject_sample_t;

static lv_indev_t   *s_indev;
static QueueHandle_t s_sample_q;
static SemaphoreHandle_t s_inject_lock;
static TaskHandle_t  s_listener_task;
static int16_t       s_last_x;
static int16_t       s_last_y;

static void indev_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    inject_sample_t s;

    if (s_sample_q != NULL &&
        xQueueReceive(s_sample_q, &s, 0) == pdTRUE) {
        s_last_x = s.x;
        s_last_y = s.y;
        data->point.x = s.x;
        data->point.y = s.y;
        data->state = s.pressed ? LV_INDEV_STATE_PRESSED
                                : LV_INDEV_STATE_RELEASED;
        if (uxQueueMessagesWaiting(s_sample_q) == 0 && s_listener_task != NULL) {
            xTaskNotifyGive(s_listener_task);
        }
    } else {
        /* Idle: hold last position, report released. */
        data->point.x = s_last_x;
        data->point.y = s_last_y;
        data->state = LV_INDEV_STATE_RELEASED;
    }
    /* Leave continue_reading false so one sample is consumed per read
     * period — this spreads a gesture over wall-clock time, which is what
     * LVGL's scroll/gesture velocity detection expects. */
}

/* ── Bounds ─────────────────────────────────────────────────────── */

static esp_err_t get_display_size(int32_t *width, int32_t *height)
{
    if (width == NULL || height == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!lvgl_port_lock(CONFIG_UITEST_LVGL_LOCK_TIMEOUT_MS)) {
        return ESP_ERR_TIMEOUT;
    }
    lv_display_t *display = lv_display_get_default();
    if (display == NULL) {
        lvgl_port_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    *width = lv_display_get_horizontal_resolution(display);
    *height = lv_display_get_vertical_resolution(display);
    lvgl_port_unlock();
    return ESP_OK;
}

static esp_err_t validate_point(int16_t x, int16_t y)
{
    int32_t width;
    int32_t height;
    esp_err_t err = get_display_size(&width, &height);
    if (err != ESP_OK) {
        return err;
    }
    return x >= 0 && y >= 0 && x < width && y < height
        ? ESP_OK
        : ESP_ERR_INVALID_ARG;
}

static esp_err_t validate_two_points(int16_t x0, int16_t y0,
                                     int16_t x1, int16_t y1)
{
    int32_t width;
    int32_t height;
    esp_err_t err = get_display_size(&width, &height);
    if (err != ESP_OK) return err;
    bool valid = x0 >= 0 && y0 >= 0 && x0 < width && y0 < height &&
                 x1 >= 0 && y1 >= 0 && x1 < width && y1 < height;
    return valid ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static esp_err_t reserve_samples(size_t sample_count)
{
    if (s_sample_q == NULL || s_inject_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (sample_count == 0 || sample_count > CONFIG_UITEST_SAMPLE_QUEUE_DEPTH) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (xSemaphoreTake(s_inject_lock,
                       pdMS_TO_TICKS(CONFIG_UITEST_DRAIN_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    TickType_t started = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(CONFIG_UITEST_DRAIN_TIMEOUT_MS);
    while (uxQueueSpacesAvailable(s_sample_q) < sample_count) {
        if ((xTaskGetTickCount() - started) >= timeout) {
            xSemaphoreGive(s_inject_lock);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }
    return ESP_OK;
}

static esp_err_t enqueue_reserved(int16_t x, int16_t y, bool pressed)
{
    inject_sample_t sample = {
        .x = x,
        .y = y,
        .pressed = pressed ? 1 : 0,
    };
    return xQueueSend(s_sample_q, &sample, 0) == pdTRUE
        ? ESP_OK
        : ESP_FAIL;
}

static void finish_samples(void)
{
    xSemaphoreGive(s_inject_lock);
}

static esp_err_t wait_drained(void)
{
    ulTaskNotifyTake(pdTRUE, 0);
    while (s_sample_q != NULL && uxQueueMessagesWaiting(s_sample_q) > 0) {
        if (ulTaskNotifyTake(pdTRUE,
                pdMS_TO_TICKS(CONFIG_UITEST_DRAIN_TIMEOUT_MS)) == 0) {
            return ESP_ERR_TIMEOUT;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(READ_PERIOD + CONFIG_UITEST_SETTLE_MS));
    return ESP_OK;
}

/* ── Public injection API ───────────────────────────────────────── */

esp_err_t uitest_tap(int16_t x, int16_t y)
{
    esp_err_t err = validate_point(x, y);
    if (err != ESP_OK) {
        return err;
    }
    err = reserve_samples(2);
    if (err != ESP_OK) {
        return err;
    }
    err = enqueue_reserved(x, y, true);
    if (err == ESP_OK) err = enqueue_reserved(x, y, false);
    finish_samples();
    return err;
}

esp_err_t uitest_long_press(int16_t x, int16_t y, uint32_t hold_ms)
{
    esp_err_t err = validate_point(x, y);
    if (err != ESP_OK || hold_ms == 0 || hold_ms > CONFIG_UITEST_MAX_GESTURE_MS) {
        return err != ESP_OK ? err : ESP_ERR_INVALID_ARG;
    }
    uint32_t steps = (hold_ms + READ_PERIOD - 1) / READ_PERIOD;
    err = reserve_samples((size_t)steps + 1);
    if (err != ESP_OK) {
        return err;
    }
    for (uint32_t i = 0; i < steps; i++) {
        err = enqueue_reserved(x, y, true);
        if (err != ESP_OK) break;
    }
    if (err == ESP_OK) err = enqueue_reserved(x, y, false);
    finish_samples();
    return err;
}

esp_err_t uitest_swipe(int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1,
                       uint32_t duration_ms)
{
    esp_err_t err = validate_two_points(x0, y0, x1, y1);
    if (err != ESP_OK || duration_ms == 0 ||
        duration_ms > CONFIG_UITEST_MAX_GESTURE_MS) {
        return err != ESP_OK ? err : ESP_ERR_INVALID_ARG;
    }
    uint32_t intervals = (duration_ms + READ_PERIOD - 1) / READ_PERIOD;
    if (intervals < 2) {
        intervals = 2;
    }
    err = reserve_samples((size_t)intervals + 2);
    if (err != ESP_OK) {
        return err;
    }
    for (uint32_t i = 0; i <= intervals; i++) {
        int32_t x = x0 + (int32_t)(x1 - x0) * (int32_t)i / (int32_t)intervals;
        int32_t y = y0 + (int32_t)(y1 - y0) * (int32_t)i / (int32_t)intervals;
        err = enqueue_reserved((int16_t)x, (int16_t)y, true);
        if (err != ESP_OK) break;
    }
    if (err == ESP_OK) err = enqueue_reserved(x1, y1, false);
    finish_samples();
    return err;
}

/* ── Widget id registry ─────────────────────────────────────────── */

typedef struct {
    lv_obj_t *obj;
    char id[CONFIG_UITEST_MAX_ID_LENGTH];
} id_entry_t;

static id_entry_t s_registry[CONFIG_UITEST_MAX_REGISTERED];

static void registry_delete_cb(lv_event_t *event)
{
    lv_obj_t *deleted = lv_event_get_target(event);
    for (int i = 0; i < CONFIG_UITEST_MAX_REGISTERED; i++) {
        if (s_registry[i].obj == deleted) {
            s_registry[i].obj = NULL;
            s_registry[i].id[0] = '\0';
        }
    }
}

esp_err_t uitest_register(lv_obj_t *obj, const char *id)
{
    if (obj == NULL || id == NULL || id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    size_t id_length = strnlen(id, CONFIG_UITEST_MAX_ID_LENGTH);
    if (id_length >= CONFIG_UITEST_MAX_ID_LENGTH) {
        return ESP_ERR_INVALID_SIZE;
    }
    for (size_t i = 0; i < id_length; i++) {
        if (isspace((unsigned char)id[i])) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (!lvgl_port_lock(CONFIG_UITEST_LVGL_LOCK_TIMEOUT_MS)) {
        return ESP_ERR_TIMEOUT;
    }

    int available = -1;
    for (int i = 0; i < CONFIG_UITEST_MAX_REGISTERED; i++) {
        if (s_registry[i].obj == obj) {
            memcpy(s_registry[i].id, id, id_length + 1);
            lvgl_port_unlock();
            return ESP_OK;
        }
        if (s_registry[i].obj != NULL && strcmp(s_registry[i].id, id) == 0) {
            lvgl_port_unlock();
            return ESP_ERR_INVALID_STATE;
        }
        if (available < 0 && s_registry[i].obj == NULL) {
            available = i;
        }
    }
    if (available < 0) {
        lvgl_port_unlock();
        return ESP_ERR_NO_MEM;
    }

    if (lv_obj_add_event_cb(obj, registry_delete_cb, LV_EVENT_DELETE, NULL) == NULL) {
        lvgl_port_unlock();
        return ESP_ERR_NO_MEM;
    }
    s_registry[available].obj = obj;
    memcpy(s_registry[available].id, id, id_length + 1);
    lvgl_port_unlock();
    return ESP_OK;
}

static lv_obj_t *registry_find_locked(const char *id)
{
    for (int i = 0; i < CONFIG_UITEST_MAX_REGISTERED; i++) {
        if (s_registry[i].obj != NULL && s_registry[i].id[0] != '\0' &&
            strcmp(s_registry[i].id, id) == 0) {
            return s_registry[i].obj;
        }
    }
    return NULL;
}

/* ── Command handling ───────────────────────────────────────────── */

static void reply_ok(const char *fmt, ...)
{
    printf("%s: OK ", RESP_PREFIX);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}

static void reply_err(const char *msg)
{
    printf("%s: ERR %s\n", RESP_PREFIX, msg);
    fflush(stdout);
}

static void reply_command_error(const char *command, esp_err_t err)
{
    printf("%s: ERR %s %s\n", RESP_PREFIX, command, esp_err_to_name(err));
    fflush(stdout);
}

static void cmd_info(void)
{
    if (!lvgl_port_lock(CONFIG_UITEST_LVGL_LOCK_TIMEOUT_MS)) {
        reply_command_error("INFO", ESP_ERR_TIMEOUT);
        return;
    }
    lv_display_t *display = lv_display_get_default();
    if (display == NULL) {
        lvgl_port_unlock();
        reply_command_error("INFO", ESP_ERR_INVALID_STATE);
        return;
    }
    int32_t w = lv_display_get_horizontal_resolution(display);
    int32_t h = lv_display_get_vertical_resolution(display);
    lv_display_rotation_t rot = lv_display_get_rotation(display);
    lvgl_port_unlock();
    reply_ok("INFO W:%d H:%d ROT:%d PERIOD:%d", (int)w, (int)h,
             (int)rot, READ_PERIOD);
}

typedef struct {
    lv_obj_t *obj;
    uint16_t depth;
} dump_pending_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    uint16_t depth;
    bool clickable;
    bool hidden;
    char id[CONFIG_UITEST_MAX_ID_LENGTH];
} dump_node_t;

static void registry_id_copy_locked(lv_obj_t *obj, char *id)
{
    strcpy(id, "-");
    for (int i = 0; i < CONFIG_UITEST_MAX_REGISTERED; i++) {
        if (s_registry[i].obj == obj && s_registry[i].id[0] != '\0') {
            strcpy(id, s_registry[i].id);
            return;
        }
    }
}

static esp_err_t dump_snapshot(dump_node_t *nodes, size_t *node_count,
                               bool *truncated)
{
    dump_pending_t *pending = calloc(CONFIG_UITEST_MAX_DUMP_NODES,
                                     sizeof(dump_pending_t));
    if (pending == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (!lvgl_port_lock(CONFIG_UITEST_LVGL_LOCK_TIMEOUT_MS)) {
        free(pending);
        return ESP_ERR_TIMEOUT;
    }

    lv_obj_t *screen = lv_screen_active();
    size_t count = 0;
    size_t pending_count = 0;
    *truncated = false;
    if (screen != NULL) {
        pending[pending_count++] = (dump_pending_t) { .obj = screen, .depth = 0 };
    }

    while (pending_count > 0 && count < CONFIG_UITEST_MAX_DUMP_NODES) {
        dump_pending_t current = pending[--pending_count];
        lv_area_t area;
        lv_obj_get_coords(current.obj, &area);

        dump_node_t *node = &nodes[count++];
        node->x = area.x1;
        node->y = area.y1;
        node->width = area.x2 - area.x1 + 1;
        node->height = area.y2 - area.y1 + 1;
        node->depth = current.depth;
        node->clickable = lv_obj_has_flag(current.obj, LV_OBJ_FLAG_CLICKABLE);
        node->hidden = !lv_obj_is_visible(current.obj);
        registry_id_copy_locked(current.obj, node->id);

        uint32_t children = lv_obj_get_child_count(current.obj);
        if (current.depth >= CONFIG_UITEST_MAX_DUMP_DEPTH) {
            if (children > 0) *truncated = true;
            continue;
        }
        for (uint32_t i = children; i > 0; i--) {
            if (pending_count >= CONFIG_UITEST_MAX_DUMP_NODES) {
                *truncated = true;
                break;
            }
            pending[pending_count++] = (dump_pending_t) {
                .obj = lv_obj_get_child(current.obj, i - 1),
                .depth = (uint16_t)(current.depth + 1),
            };
        }
    }
    if (pending_count > 0) *truncated = true;
    lvgl_port_unlock();
    free(pending);
    *node_count = count;
    return ESP_OK;
}

static void cmd_dump(void)
{
    dump_node_t *nodes = calloc(CONFIG_UITEST_MAX_DUMP_NODES,
                                sizeof(dump_node_t));
    if (nodes == NULL) {
        printf("---%s_DUMP_START---\n", RESP_PREFIX);
        printf("%s: ERR DUMP %s\n", RESP_PREFIX,
               esp_err_to_name(ESP_ERR_NO_MEM));
        printf("---%s_DUMP_END---\n", RESP_PREFIX);
        fflush(stdout);
        return;
    }
    size_t node_count = 0;
    bool truncated = false;
    esp_err_t err = dump_snapshot(nodes, &node_count, &truncated);

    printf("---%s_DUMP_START---\n", RESP_PREFIX);
    if (err == ESP_OK) {
        uint32_t crc = 0;
        char line[CONFIG_UITEST_MAX_ID_LENGTH + 128];
        for (size_t i = 0; i < node_count; i++) {
            dump_node_t *node = &nodes[i];
            int length = snprintf(
                line, sizeof(line),
                "%s: NODE d:%u id:%s x:%ld y:%ld w:%ld h:%ld click:%d hidden:%d\n",
                RESP_PREFIX, node->depth, node->id,
                (long)node->x, (long)node->y,
                (long)node->width, (long)node->height,
                node->clickable, node->hidden);
            if (length < 0 || (size_t)length >= sizeof(line)) {
                err = ESP_ERR_INVALID_SIZE;
                break;
            }
            fwrite(line, 1, (size_t)length, stdout);
            crc = esp_rom_crc32_le(crc, (const uint8_t *)line, (uint32_t)length);
        }
        if (err == ESP_OK && truncated) {
            printf("%s: WARN DUMP truncated at %d nodes/depth %d\n",
                   RESP_PREFIX, CONFIG_UITEST_MAX_DUMP_NODES,
                   CONFIG_UITEST_MAX_DUMP_DEPTH);
        }
        if (err == ESP_OK) {
            printf("%s: DUMP CRC32:%08lx COUNT:%lu TRUNCATED:%d\n",
                   RESP_PREFIX, (unsigned long)crc,
                   (unsigned long)node_count, truncated);
        }
    }
    if (err != ESP_OK) {
        printf("%s: ERR DUMP %s\n", RESP_PREFIX, esp_err_to_name(err));
    }
    printf("---%s_DUMP_END---\n", RESP_PREFIX);
    fflush(stdout);
    free(nodes);
}

/* Center of a registered/clickable object -> a tap. */
static esp_err_t click_id(const char *id)
{
    if (!lvgl_port_lock(CONFIG_UITEST_LVGL_LOCK_TIMEOUT_MS)) {
        return ESP_ERR_TIMEOUT;
    }
    lv_obj_t *obj = registry_find_locked(id);
    lv_area_t a = {0};
    bool actionable = obj != NULL && lv_obj_is_visible(obj) &&
        lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE) &&
        lv_obj_get_screen(obj) == lv_screen_active();
    if (actionable) {
        lv_obj_get_coords(obj, &a);
    }
    lvgl_port_unlock();

    if (obj == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!actionable) {
        return ESP_ERR_INVALID_STATE;
    }
    int16_t cx = (int16_t)((a.x1 + a.x2) / 2);
    int16_t cy = (int16_t)((a.y1 + a.y2) / 2);
    return uitest_tap(cx, cy);
}

static const char *skip_word(const char *cursor)
{
    while (isspace((unsigned char)*cursor)) cursor++;
    while (*cursor != '\0' && !isspace((unsigned char)*cursor)) cursor++;
    while (isspace((unsigned char)*cursor)) cursor++;
    return cursor;
}

static bool command_has_no_args(const char *line)
{
    return *skip_word(line) == '\0';
}

static bool parse_int_args(const char *line, int32_t *values, size_t count)
{
    const char *cursor = skip_word(line);
    for (size_t i = 0; i < count; i++) {
        if (*cursor == '\0') return false;
        errno = 0;
        char *end;
        long value = strtol(cursor, &end, 10);
        if (end == cursor || errno == ERANGE || value < INT32_MIN || value > INT32_MAX) {
            return false;
        }
        values[i] = (int32_t)value;
        cursor = end;
        while (isspace((unsigned char)*cursor)) cursor++;
    }
    return *cursor == '\0';
}

static bool parse_id_arg(const char *line, char *id, size_t id_size)
{
    const char *cursor = skip_word(line);
    const char *end = cursor;
    while (*end != '\0' && !isspace((unsigned char)*end)) end++;
    size_t length = (size_t)(end - cursor);
    while (isspace((unsigned char)*end)) end++;
    if (length == 0 || length >= id_size || *end != '\0') return false;
    memcpy(id, cursor, length);
    id[length] = '\0';
    return true;
}

static bool parse_verb(const char *line, char *verb, size_t verb_size)
{
    while (isspace((unsigned char)*line)) line++;
    const char *end = line;
    while (*end != '\0' && !isspace((unsigned char)*end)) end++;
    size_t length = (size_t)(end - line);
    if (length == 0 || length >= verb_size) return false;
    memcpy(verb, line, length);
    verb[length] = '\0';
    return true;
}

static esp_err_t point_from_int32(int32_t x, int32_t y,
                                  int16_t *point_x, int16_t *point_y)
{
    if (x < INT16_MIN || x > INT16_MAX || y < INT16_MIN || y > INT16_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    *point_x = (int16_t)x;
    *point_y = (int16_t)y;
    return ESP_OK;
}

static void finish_gesture_command(const char *command, esp_err_t err,
                                   const char *reply_format, ...)
{
    if (err == ESP_OK) err = wait_drained();
    if (err != ESP_OK) {
        reply_command_error(command, err);
        return;
    }

    printf("%s: OK ", RESP_PREFIX);
    va_list args;
    va_start(args, reply_format);
    vprintf(reply_format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

/* Parse and dispatch one command line. */
static void handle_line(char *line)
{
    char verb[CONFIG_UITEST_COMMAND_LINE_LENGTH];
    if (!parse_verb(line, verb, sizeof(verb))) {
        return; /* blank line */
    }

    if (strcmp(verb, "INFO") == 0 && command_has_no_args(line)) {
        cmd_info();

    } else if (strcmp(verb, "DUMP") == 0 && command_has_no_args(line)) {
        esp_err_t err = wait_drained();
        if (err != ESP_OK) {
            reply_command_error("DUMP", err);
            return;
        }
        cmd_dump();

    } else if (strcmp(verb, "TAP") == 0) {
        int32_t args[2];
        int16_t x;
        int16_t y;
        if (!parse_int_args(line, args, 2) ||
            point_from_int32(args[0], args[1], &x, &y) != ESP_OK) {
            reply_err("TAP <x> <y>");
            return;
        }
        finish_gesture_command("TAP", uitest_tap(x, y),
                               "TAP %ld %ld", (long)args[0], (long)args[1]);

    } else if (strcmp(verb, "LONGPRESS") == 0) {
        int32_t args[3];
        int16_t x;
        int16_t y;
        if (!parse_int_args(line, args, 3) || args[2] <= 0 ||
            point_from_int32(args[0], args[1], &x, &y) != ESP_OK) {
            reply_err("LONGPRESS <x> <y> <ms>");
            return;
        }
        finish_gesture_command("LONGPRESS",
                               uitest_long_press(x, y, (uint32_t)args[2]),
                               "LONGPRESS %ld %ld %ld",
                               (long)args[0], (long)args[1], (long)args[2]);

    } else if (strcmp(verb, "SWIPE") == 0) {
        int32_t args[5];
        int16_t x0;
        int16_t y0;
        int16_t x1;
        int16_t y1;
        if (!parse_int_args(line, args, 5) || args[4] <= 0 ||
            point_from_int32(args[0], args[1], &x0, &y0) != ESP_OK ||
            point_from_int32(args[2], args[3], &x1, &y1) != ESP_OK) {
            reply_err("SWIPE <x0> <y0> <x1> <y1> <ms>");
            return;
        }
        finish_gesture_command("SWIPE",
                               uitest_swipe(x0, y0, x1, y1, (uint32_t)args[4]),
                               "SWIPE %ld %ld %ld %ld %ld",
                               (long)args[0], (long)args[1],
                               (long)args[2], (long)args[3], (long)args[4]);

    } else if (strcmp(verb, "CLICK") == 0) {
        char id[CONFIG_UITEST_MAX_ID_LENGTH];
        if (parse_id_arg(line, id, sizeof(id))) {
            esp_err_t err = click_id(id);
            finish_gesture_command("CLICK", err, "CLICK %s", id);
        } else {
            reply_err("CLICK <id>");
        }

    } else if (strcmp(verb, "SHOT") == 0 && command_has_no_args(line)) {
#ifdef CONFIG_SCREENSHOT_ENABLED
        esp_err_t err = screenshot_take();
        if (err == ESP_OK) reply_ok("SHOT");
        else reply_command_error("SHOT", err);
#else
        reply_err("SHOT screenshot component disabled");
#endif

    } else {
        printf("%s: ERR %s unknown command or invalid arguments\n",
               RESP_PREFIX, verb);
        fflush(stdout);
    }
}

static void listener_task(void *arg)
{
    (void)arg;
    char line[CONFIG_UITEST_COMMAND_LINE_LENGTH];
    size_t length = 0;
    bool overflow = false;

    ESP_LOGI(TAG, "UI test harness ready — commands: "
                  "INFO DUMP TAP LONGPRESS SWIPE CLICK SHOT");

    for (;;) {
        int character = fgetc(stdin);
        if (character == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (character == '\r') continue;
        if (character == '\n') {
            if (overflow) {
                reply_err("command too long");
            } else if (length > 0) {
                line[length] = '\0';
                handle_line(line);
            }
            length = 0;
            overflow = false;
            continue;
        }
        if (length + 1 < sizeof(line)) {
            line[length++] = (char)character;
        } else {
            overflow = true;
        }
    }
}

/* ── Initialization ─────────────────────────────────────────────── */

esp_err_t uitest_init(void)
{
    if (RESP_PREFIX[0] == '\0' || strpbrk(RESP_PREFIX, ":\r\n\t ") != NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_listener_task != NULL) return ESP_OK;

    if (s_indev == NULL) {
        s_sample_q = xQueueCreate(CONFIG_UITEST_SAMPLE_QUEUE_DEPTH,
                                  sizeof(inject_sample_t));
        if (s_sample_q == NULL) {
            ESP_LOGE(TAG, "Failed to allocate sample queue");
            return ESP_ERR_NO_MEM;
        }

        s_inject_lock = xSemaphoreCreateMutex();
        if (s_inject_lock == NULL) {
            ESP_LOGE(TAG, "Failed to allocate injection mutex");
            vQueueDelete(s_sample_q);
            s_sample_q = NULL;
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_indev == NULL) {
        if (!lvgl_port_lock(CONFIG_UITEST_LVGL_LOCK_TIMEOUT_MS)) {
            ESP_LOGE(TAG, "Timed out acquiring LVGL lock during initialization");
            vSemaphoreDelete(s_inject_lock);
            vQueueDelete(s_sample_q);
            s_inject_lock = NULL;
            s_sample_q = NULL;
            return ESP_ERR_TIMEOUT;
        }
        lv_display_t *display = lv_display_get_default();
        if (display == NULL) {
            lvgl_port_unlock();
            vSemaphoreDelete(s_inject_lock);
            vQueueDelete(s_sample_q);
            s_inject_lock = NULL;
            s_sample_q = NULL;
            return ESP_ERR_INVALID_STATE;
        }

        s_last_x = (int16_t)(lv_display_get_horizontal_resolution(display) / 2);
        s_last_y = (int16_t)(lv_display_get_vertical_resolution(display) / 2);
        s_indev = lv_indev_create();
        if (s_indev == NULL) {
            lvgl_port_unlock();
            ESP_LOGE(TAG, "Failed to create synthetic indev");
            vSemaphoreDelete(s_inject_lock);
            vQueueDelete(s_sample_q);
            s_inject_lock = NULL;
            s_sample_q = NULL;
            return ESP_ERR_NO_MEM;
        }
        lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_indev, indev_read_cb);
        lv_indev_set_display(s_indev, display);
        lv_timer_t *read_timer = lv_indev_get_read_timer(s_indev);
        if (read_timer == NULL) {
            lv_indev_delete(s_indev);
            s_indev = NULL;
            lvgl_port_unlock();
            vSemaphoreDelete(s_inject_lock);
            vQueueDelete(s_sample_q);
            s_inject_lock = NULL;
            s_sample_q = NULL;
            return ESP_ERR_INVALID_STATE;
        }
        lv_timer_set_period(read_timer, READ_PERIOD);
        lvgl_port_unlock();
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        listener_task, "uitest", CONFIG_UITEST_TASK_STACK_SIZE,
        NULL, 2, &s_listener_task, 0);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to start listener task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UI test harness initialized");
    return ESP_OK;
}

#else

esp_err_t uitest_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t uitest_register(lv_obj_t *obj, const char *id)
{
    (void)obj;
    (void)id;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t uitest_tap(int16_t x, int16_t y)
{
    (void)x;
    (void)y;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t uitest_long_press(int16_t x, int16_t y, uint32_t hold_ms)
{
    (void)x;
    (void)y;
    (void)hold_ms;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t uitest_swipe(int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1,
                       uint32_t duration_ms)
{
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)duration_ms;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif /* CONFIG_UITEST_ENABLED */
