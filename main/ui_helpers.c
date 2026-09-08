/*
 * ui_helpers.c — Reusable LVGL v9 card-based UI component system
 *
 * All layout uses LV_LAYOUT_FLEX so that coordinates are computed
 * automatically.  No manual lv_obj_align / lv_obj_set_pos calls.
 */

#include "ui_helpers.h"
#include "uitest.h"
#include "esp_log.h"

void ui_test_id(lv_obj_t *obj, const char *id)
{
#ifdef CONFIG_UITEST_ENABLED
    esp_err_t err = uitest_register(obj, id);
    if (err != ESP_OK) ESP_LOGE("UI", "Widget ID %s: %s", id, esp_err_to_name(err));
#else
    (void)obj;
    (void)id;
#endif
}

/* ── Tab page helper ───────────────────────────────────────────────────── */

lv_obj_t *ui_tabview_add_tab( lv_obj_t *tv, const char *name )
{
    lv_obj_t *tab = lv_tabview_add_tab( tv, name );
    ui_test_id(tab, name);
    lv_obj_set_style_pad_all( tab, 0, 0 );
    lv_obj_set_style_bg_opa( tab, LV_OPA_TRANSP, 0 );

    /* Tab page is a flex column so the card fills it with margin */
    lv_obj_set_layout( tab, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( tab, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( tab, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    return tab;
}

/* ── Card creation ─────────────────────────────────────────────────────── */

lv_obj_t *ui_create_card( lv_obj_t *parent, lv_color_t bg_color )
{
    lv_obj_t *card = lv_obj_create( parent );
    lv_obj_remove_flag( card, LV_OBJ_FLAG_SCROLLABLE );

    /* Size: fill parent width with margin, fixed height */
    lv_obj_set_size( card, 290, 170 );

    /* Rounded corners, colored background */
    lv_obj_set_style_radius( card, UI_CARD_RADIUS, 0 );
    lv_obj_set_style_bg_color( card, bg_color, 0 );
    lv_obj_set_style_bg_opa( card, LV_OPA_COVER, 0 );
    lv_obj_set_style_border_width( card, 0, 0 );

    /* Flex-column layout for children: title → description → content */
    lv_obj_set_layout( card, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( card, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_all( card, UI_CARD_PAD, 0 );
    lv_obj_set_style_pad_row( card, 6, 0 );

    return card;
}

/* ── Card child helpers ────────────────────────────────────────────────── */

lv_obj_t *ui_card_title( lv_obj_t *card, const char *text, lv_color_t color )
{
    lv_obj_t *label = lv_label_create( card );
    lv_label_set_text_static( label, text );
    lv_obj_set_style_text_color( label, color, 0 );
    lv_obj_set_width( label, lv_pct( 100 ) );
    lv_obj_set_style_text_align( label, LV_TEXT_ALIGN_CENTER, 0 );
    return label;
}

lv_obj_t *ui_card_text( lv_obj_t *card, const char *text, lv_color_t color )
{
    lv_obj_t *label = lv_label_create( card );
    lv_label_set_text_static( label, text );
    lv_label_set_long_mode( label, LV_LABEL_LONG_WRAP );
    lv_obj_set_style_text_color( label, color, 0 );
    lv_obj_set_width( label, lv_pct( 100 ) );
    return label;
}
