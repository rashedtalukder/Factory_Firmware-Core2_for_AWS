/*
 * ui_helpers.h — Reusable LVGL v9 card-based UI component system
 *
 * Provides a standardized card layout for all tabs:
 *   tab → card (flex column) → title, description, content area
 *
 * Uses LV_LAYOUT_FLEX throughout to eliminate manual coordinate arithmetic.
 */

#pragma once

#include "lvgl.h"

/* ── Design tokens ─────────────────────────────────────────────────────── */
#define UI_SCREEN_BG_COLOR   0x1a1a2e   /* dark navy background           */
#define UI_ACCENT_COLOR      0xff9900   /* Amazon Orange                   */
#define UI_DOT_INACTIVE      0x555555   /* inactive page-dot color         */
#define UI_CARD_RADIUS       12
#define UI_CARD_PAD          14         /* inner padding of a card         */

/* ── Card creation ─────────────────────────────────────────────────────── */

/**
 * Create a standard card panel inside a tab page.
 *
 * The card is a flex-column container that fills its parent with a fixed
 * margin.  Its background color is set to `bg_color`.
 *
 * @param parent   The tab page object (from lv_tabview_add_tab).
 * @param bg_color Card background color.
 * @return         The card container — add children to this.
 */
lv_obj_t *ui_create_card( lv_obj_t *parent, lv_color_t bg_color );

/* ── Card child helpers ────────────────────────────────────────────────── */

/**
 * Add a centered title label inside a card.
 */
lv_obj_t *ui_card_title( lv_obj_t *card, const char *text, lv_color_t color );

/**
 * Add a wrapped description label inside a card.
 * The label fills the card width minus padding.
 */
lv_obj_t *ui_card_text( lv_obj_t *card, const char *text, lv_color_t color );

/* ── Tab page helper ───────────────────────────────────────────────────── */

/**
 * Add a tab to the tabview and apply the standard transparent style.
 * Returns the tab page object ready for ui_create_card().
 */
lv_obj_t *ui_tabview_add_tab( lv_obj_t *tv, const char *name );
