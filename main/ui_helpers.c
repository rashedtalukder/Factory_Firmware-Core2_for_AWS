/*
 * ui_helpers.c — Reusable LVGL v9 card-based UI component system
 *
 * All layout uses LV_LAYOUT_FLEX so that coordinates are computed
 * automatically.  No manual lv_obj_align / lv_obj_set_pos calls.
 */

#include "ui_helpers.h"
#include "uitest.h"
#include "esp_log.h"

static lv_style_t card_style;
static lv_style_t title_style;
static bool styles_ready;

static void ui_styles_init(void)
{
    if (styles_ready) return;

    lv_style_init(&card_style);
    lv_style_set_radius(&card_style, UI_CARD_RADIUS);
    lv_style_set_bg_opa(&card_style, LV_OPA_COVER);
    lv_style_set_border_width(&card_style, 0);
    lv_style_set_pad_all(&card_style, UI_CARD_PAD);
    lv_style_set_pad_row(&card_style, 6);

    lv_style_init(&title_style);
    lv_style_set_text_align(&title_style, LV_TEXT_ALIGN_CENTER);
    styles_ready = true;
}

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
    ui_styles_init();
    lv_obj_t *card = lv_obj_create( parent );
    lv_obj_set_scrollable( card, false );

    /* Size: fill parent width with margin, fixed height */
    lv_obj_set_size( card, 290, 170 );

    /* Rounded corners, colored background */
    lv_obj_add_style( card, &card_style, 0 );
    lv_obj_set_style_bg_color( card, bg_color, 0 );

    /* Flex-column layout for children: title → description → content */
    lv_obj_set_layout( card, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( card, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );

    return card;
}

/* ── Card child helpers ────────────────────────────────────────────────── */

lv_obj_t *ui_card_title( lv_obj_t *card, const char *text, lv_color_t color )
{
    ui_styles_init();
    lv_obj_t *label = lv_label_create( card );
    lv_obj_add_style( label, &title_style, 0 );
    lv_label_set_text_static( label, text );
    lv_obj_set_style_text_color( label, color, 0 );
    lv_obj_set_width( label, lv_pct( 100 ) );
    return label;
}

lv_obj_t *ui_card_text( lv_obj_t *card, const char *text, lv_color_t color )
{
    lv_obj_t *label = lv_label_create( card );
    lv_label_set_text_static( label, text );
    lv_label_set_long_mode( label, LV_LABEL_LONG_MODE_WRAP );
    lv_obj_set_style_text_color( label, color, 0 );
    lv_obj_set_width( label, lv_pct( 100 ) );
    return label;
}

lv_obj_t *ui_create_row( lv_obj_t *parent, lv_flex_align_t main_align, int32_t gap )
{
    lv_obj_t *row = lv_obj_create( parent );
    lv_obj_remove_style_all( row );
    lv_obj_set_size( row, lv_pct( 100 ), LV_SIZE_CONTENT );
    lv_obj_set_layout( row, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( row, LV_FLEX_FLOW_ROW );
    lv_obj_set_flex_align( row, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_column( row, gap, 0 );
    return row;
}

lv_obj_t *ui_card_action( lv_obj_t *card, const char *prompt, lv_color_t color, lv_flex_align_t main_align )
{
    /* Grows to fill the card and bottom-anchors its children so prompts line up across tabs. */
    lv_obj_t *section = lv_obj_create( card );
    lv_obj_remove_style_all( section );
    lv_obj_set_width( section, lv_pct( 100 ) );
    lv_obj_set_flex_grow( section, 1 );
    lv_obj_set_layout( section, LV_LAYOUT_FLEX );
    lv_obj_set_flex_flow( section, LV_FLEX_FLOW_COLUMN );
    lv_obj_set_flex_align( section, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER );
    lv_obj_set_style_pad_row( section, UI_ACTION_GAP, 0 );

    lv_obj_t *label = lv_label_create( section );
    lv_label_set_text_static( label, prompt );
    lv_obj_set_style_text_color( label, color, 0 );
    lv_obj_set_style_text_align( label, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_set_width( label, lv_pct( 100 ) );

    lv_obj_t *content = ui_create_row( section, main_align, 0 );
    lv_obj_set_height( content, UI_ACTION_HEIGHT );
    lv_obj_set_scrollable( content, false );
    return content;
}

lv_obj_t *ui_value_label( lv_obj_t *parent, const char *text )
{
    lv_obj_t *label = lv_label_create( parent );
    lv_label_set_text( label, text );
    lv_obj_set_width( label, UI_VALUE_WIDTH );
    lv_obj_set_style_text_align( label, LV_TEXT_ALIGN_CENTER, 0 );
    lv_obj_set_style_text_color( label, lv_color_white(), 0 );
    lv_obj_set_style_bg_color( label, lv_color_black(), 0 );
    lv_obj_set_style_bg_opa( label, LV_OPA_70, 0 );
    lv_obj_set_style_radius( label, 6, 0 );
    lv_obj_set_style_pad_hor( label, 8, 0 );
    lv_obj_set_style_pad_ver( label, 5, 0 );
    return label;
}

static void ui_overlay_deleted( lv_event_t *event )
{
    lv_obj_t **owner = lv_event_get_user_data( event );
    if ( *owner == lv_event_get_target_obj( event ) ) *owner = NULL;
}

lv_obj_t *ui_overlay_create( lv_obj_t **owner, lv_color_t color, lv_opa_t opacity )
{
    lv_obj_t *overlay = lv_obj_create( lv_screen_active() );
    lv_obj_remove_style_all( overlay );
    lv_obj_set_floating( overlay, true );
    lv_obj_set_size( overlay, lv_pct( 100 ), lv_pct( 100 ) );
    lv_obj_set_scrollable( overlay, false );
    lv_obj_set_style_bg_color( overlay, color, 0 );
    lv_obj_set_style_bg_opa( overlay, opacity, 0 );
    *owner = overlay;
    lv_obj_add_event_cb( overlay, ui_overlay_deleted, LV_EVENT_DELETE, owner );
    return overlay;
}

void ui_overlay_close( lv_obj_t **overlay )
{
    if (*overlay == NULL) return;
    lv_obj_delete_async( *overlay );
    *overlay = NULL;
}

lv_obj_t *ui_dialog_create( lv_obj_t **overlay, const char *title, const char *text )
{
    ui_overlay_create( overlay, lv_color_black(), LV_OPA_50 );
    lv_obj_t *dialog = lv_msgbox_create( *overlay );
    lv_obj_set_width( dialog, lv_pct( 90 ) );
    lv_obj_center( dialog );
    lv_msgbox_add_title( dialog, title );
    lv_msgbox_add_text( dialog, text );
    return dialog;
}

void ui_dialog_add_button( lv_obj_t *dialog, const char *text, const char *id, lv_event_cb_t cb )
{
    lv_obj_t *button = lv_msgbox_add_footer_button( dialog, text );
    ui_test_id( button, id );
    lv_obj_add_event_cb( button, cb, LV_EVENT_CLICKED, NULL );
}
