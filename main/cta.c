/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v2.4.0
 * home.c
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
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "core2foraws.h"

#include "ui_helpers.h"
#include "cta.h"
#include "screenshot.h"
#include "esp_heap_caps.h"

static const char *TAG = CTA_TAB_NAME;
static lv_obj_t *diagnostics_panel;

static void diagnostics_refresh(lv_event_t *event)
{
    lv_obj_t *label = lv_event_get_user_data(event);
    screenshot_stats_t stats = {0};
    esp_err_t result = screenshot_start();
    screenshot_get_stats(&stats);
    lv_label_set_text_fmt(label,
        "Hardware initialized\nInternal: %u bytes\nDMA: %u bytes\nCaptures: %lu / errors: %lu\nLast: %lu ms\nCapture: %s",
        (unsigned int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned int)heap_caps_get_free_size(MALLOC_CAP_DMA),
        (unsigned long)stats.completed, (unsigned long)stats.failed,
        (unsigned long)stats.last_duration_ms, esp_err_to_name(result));
}

static void diagnostics_close(lv_event_t *event)
{
    (void)event;
    ui_overlay_close(&diagnostics_panel);
}

static void diagnostics_open(void)
{
    if (diagnostics_panel != NULL) return;
    lv_obj_t *panel = ui_overlay_create(&diagnostics_panel, lv_color_hex(UI_SCREEN_BG_COLOR), LV_OPA_COVER);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Hardware initialized");
    ui_test_id(label, "diagnostics.status");
    lv_obj_t *row = lv_obj_create(panel);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 248, 36);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *refresh = lv_button_create(row);
    lv_obj_set_size(refresh, 112, 34);
    lv_label_set_text(lv_label_create(refresh), LV_SYMBOL_REFRESH " Refresh");
    lv_obj_add_event_cb(refresh, diagnostics_refresh, LV_EVENT_CLICKED, label);
    ui_test_id(refresh, "diagnostics.refresh");
    lv_obj_t *close = lv_button_create(row);
    lv_obj_set_size(close, 112, 34);
    lv_label_set_text(lv_label_create(close), LV_SYMBOL_CLOSE " Close");
    lv_obj_add_event_cb(close, diagnostics_close, LV_EVENT_CLICKED, NULL);
    ui_test_id(close, "diagnostics.close");
    lv_obj_send_event(refresh, LV_EVENT_CLICKED, NULL);
}

void display_cta_tab( lv_obj_t *tv )
{
    lvgl_port_lock( 0 );
    
    lv_obj_t *cta_tab = ui_tabview_add_tab( tv, CTA_TAB_NAME );

    /* Card with flex-column layout */
    lv_obj_t *card = ui_create_card( cta_tab, lv_color_make( 35, 47, 62 ) );
    ui_card_title( card, "Next Steps", lv_color_make(255,255,255) );
    ui_card_text( card, "Get hands-on experience building IoT solutions with AWS IoT Kit:", lv_color_make(255,255,255) );

    /* Equal spacers above and below center the URL in the remaining space */
    lv_obj_t *spacer = lv_obj_create( card );
    lv_obj_remove_style_all( spacer );
    lv_obj_set_size( spacer, 0, 0 );
    lv_obj_set_flex_grow( spacer, 1 );

    lv_obj_t *url_label = lv_label_create( card );
    lv_obj_set_style_text_color( url_label, lv_color_hex( UI_ACCENT_COLOR ), 0 );
    lv_label_set_text( url_label, "https://aws-iot-kit-docs.m5stack.com" );
    /* Content width keeps it on one line; it is ~2 px wider than the padded card */
    lv_obj_set_width( url_label, LV_SIZE_CONTENT );

    lv_obj_t *bottom_spacer = lv_obj_create( card );
    lv_obj_remove_style_all( bottom_spacer );
    lv_obj_set_size( bottom_spacer, 0, 0 );
    lv_obj_set_flex_grow( bottom_spacer, 1 );

    /* Same geometry as the Touch page's right-button line, so it sits above that button */
    static lv_point_precise_t line_points[] = { {20, 0}, {70, 0} };
    lv_obj_t *line_row = lv_obj_create( cta_tab );
    lv_obj_remove_style_all( line_row );
    lv_obj_set_size( line_row, lv_pct( 90 ), 10 );
    lv_obj_set_layout( line_row, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( line_row, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( line_row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );

    lv_obj_t *right_line = lv_line_create( line_row );
    lv_line_set_points( right_line, line_points, 2 );
    lv_obj_set_style_line_width( right_line, 6, 0 );
    lv_obj_set_style_line_color( right_line, lv_color_hex( UI_DOT_INACTIVE ), 0 );
    lv_obj_set_style_line_rounded( right_line, true, 0 );
    
    lvgl_port_unlock();

    ESP_LOGD( TAG, "Building tab" );
}

void cta_on_right_press( void )
{
    if ( !lvgl_port_lock( 1000 ) ) return;
    diagnostics_open();
    lvgl_port_unlock();
}