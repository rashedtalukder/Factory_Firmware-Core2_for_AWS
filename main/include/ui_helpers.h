/*
 * AWS IoT Kit - M5Stack Core2
 * Factory Firmware v3.0.0
 * ui_helpers.h
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
 * @file ui_helpers.h
 * @brief Reusable LVGL v9 card-based UI component and overlay helpers.
 *
 * Provides a standardized flex-column card layout for all tabs:
 *   tab -> card (flex column) -> title, description, content area
 */

#pragma once

#include "lvgl.h"

/**
 * @brief Register an LVGL widget with the UI test harness if enabled.
 *
 * @param obj Target LVGL object.
 * @param id  Stable string identifier for automated UI queries and clicks.
 */
void ui_test_id(lv_obj_t *obj, const char *id);

/* ── Design tokens ─────────────────────────────────────────────────────── */

/** @brief Screen background color (dark navy). */
#define UI_SCREEN_BG_COLOR   0x1a1a2e
/** @brief Primary accent color (Amazon Orange). */
#define UI_ACCENT_COLOR      0xff9900
/** @brief Inactive page-indicator dot color. */
#define UI_DOT_INACTIVE      0x555555
/** @brief Corner radius in pixels for standard cards. */
#define UI_CARD_RADIUS       12
/** @brief Inner padding in pixels for standard cards. */
#define UI_CARD_PAD          14
/** @brief Vertical gap in pixels between an action prompt and its controls row. */
#define UI_ACTION_GAP        6
/** @brief Fixed height in pixels of a card's bottom action row. */
#define UI_ACTION_HEIGHT     44
/** @brief Standard width in pixels of a dark value/status pill label. */
#define UI_VALUE_WIDTH       200

/* ── Card creation ─────────────────────────────────────────────────────── */

/**
 * @brief Create a standard card panel inside a tab page.
 *
 * The card is a flex-column container that fills its parent with a fixed
 * margin. Its background color is set to @p bg_color.
 *
 * @param parent   The tab page object (from ui_tabview_add_tab).
 * @param bg_color Card background color.
 * @return         The card container — add children to this.
 */
lv_obj_t *ui_create_card( lv_obj_t *parent, lv_color_t bg_color );

/* ── Card child helpers ────────────────────────────────────────────────── */

/**
 * @brief Add a centered title label inside a card.
 *
 * @param card  Parent card container.
 * @param text  Static title string.
 * @param color Text color.
 * @return      Created label object.
 */
lv_obj_t *ui_card_title( lv_obj_t *card, const char *text, lv_color_t color );

/**
 * @brief Add a wrapped description label inside a card.
 *
 * The label fills the card width minus padding.
 *
 * @param card  Parent card container.
 * @param text  Static body text string.
 * @param color Text color.
 * @return      Created label object.
 */
lv_obj_t *ui_card_text( lv_obj_t *card, const char *text, lv_color_t color );

/**
 * @brief Add the card's bottom-anchored action section.
 *
 * Creates a centered prompt above a fixed-height flex row.
 *
 * @param card       Parent card container.
 * @param prompt     Static prompt label text.
 * @param color      Prompt text color.
 * @param main_align Main-axis flex alignment for the returned control row.
 * @return           The action row container — add controls or outputs to it.
 */
lv_obj_t *ui_card_action( lv_obj_t *card, const char *prompt, lv_color_t color, lv_flex_align_t main_align );

/**
 * @brief Create a centered output label on a translucent dark pill.
 *
 * @param parent Parent container (typically an action row).
 * @param text   Initial text string (copied by LVGL).
 * @return       Created value label object.
 */
lv_obj_t *ui_value_label( lv_obj_t *parent, const char *text );

/**
 * @brief Add a full-width, content-height horizontal flex row to a container.
 *
 * @param parent     Parent container.
 * @param main_align Main-axis flex alignment.
 * @param gap        Column gap in pixels between children.
 * @return           Created row container.
 */
lv_obj_t *ui_create_row( lv_obj_t *parent, lv_flex_align_t main_align, int32_t gap );

/**
 * @brief Create a full-screen floating overlay on the active screen.
 *
 * Automatically clears @p *owner when the overlay is deleted.
 *
 * @param owner   Pointer to the caller's handle tracking the overlay.
 * @param color   Background color of the overlay.
 * @param opacity Background opacity.
 * @return        Created overlay container.
 */
lv_obj_t *ui_overlay_create( lv_obj_t **owner, lv_color_t color, lv_opa_t opacity );

/**
 * @brief Asynchronously close and clear an active screen overlay if open.
 *
 * @param overlay Pointer to the caller's overlay handle.
 */
void ui_overlay_close( lv_obj_t **overlay );

/**
 * @brief Create a modal message-box dialog inside a dimmed full-screen backdrop.
 *
 * @param overlay Pointer to the caller's backdrop handle.
 * @param title   Dialog title text.
 * @param text    Dialog body text.
 * @return        Created message-box widget.
 */
lv_obj_t *ui_dialog_create( lv_obj_t **overlay, const char *title, const char *text );

/**
 * @brief Add a footer button to a modal dialog and register its UI test ID.
 *
 * @param dialog Target message-box widget.
 * @param text   Button label text.
 * @param id     UI test identifier string.
 * @param cb     Click event callback.
 */
void ui_dialog_add_button( lv_obj_t *dialog, const char *text, const char *id, lv_event_cb_t cb );

/**
 * @brief Add a row of up to three capacitive-button indicator lines below a card.
 *
 * Pass 0 for any indicator that should remain hidden.
 *
 * @param parent       Tab page container below the card.
 * @param left_color   24-bit RGB hex color for the left indicator (0 to hide).
 * @param middle_color 24-bit RGB hex color for the middle indicator (0 to hide).
 * @param right_color  24-bit RGB hex color for the right indicator (0 to hide).
 * @return             Created indicator row container.
 */
lv_obj_t *ui_button_indicators_create( lv_obj_t *parent, uint32_t left_color,
                                       uint32_t middle_color, uint32_t right_color );

/* ── Tab page helper ───────────────────────────────────────────────────── */

/**
 * @brief Add a tab to the tabview and apply the standard transparent flex style.
 *
 * @param tv   Target LVGL tabview object.
 * @param name Tab identifier (also registered as its UI test ID).
 * @return     The tab page object ready for ui_create_card().
 */
lv_obj_t *ui_tabview_add_tab( lv_obj_t *tv, const char *name );
