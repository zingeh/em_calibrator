/*
 * ui_styles.c — ElecholicTech design-system style definitions
 *
 * Matches DESIGN-elecholicTech.md front-matter tokens exactly.
 * Every interactive element: 2 px radius, no shadows.
 * Pressed states follow "button-*-active" component specs.
 */

#include "ui_styles.h"

/* ---- Global style objects ---- */
lv_style_t ui_style_header;
lv_style_t ui_style_card;
lv_style_t ui_style_btn_primary;
lv_style_t ui_style_btn_primary_pressed;
lv_style_t ui_style_btn_secondary;
lv_style_t ui_style_btn_secondary_pressed;
lv_style_t ui_style_btn_danger;
lv_style_t ui_style_btn_danger_pressed;
lv_style_t ui_style_btn_disabled;
lv_style_t ui_style_label_value;
lv_style_t ui_style_label_medium;
lv_style_t ui_style_label_small;
lv_style_t ui_style_label_caption;
lv_style_t ui_style_section_title;
lv_style_t ui_style_online_dot;
lv_style_t ui_style_offline_dot;

void ui_styles_init(void)
{
    /* ================================================================ */
    /* Header bar  (surface-dark bg, white text, zero radius)           */
    /* ================================================================ */
    lv_style_init(&ui_style_header);
    lv_style_set_bg_color(&ui_style_header, UI_COLOR_SURFACE_DARK);
    lv_style_set_bg_opa(&ui_style_header, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_header, UI_COLOR_ON_DARK);
    lv_style_set_border_width(&ui_style_header, 0);
    lv_style_set_radius(&ui_style_header, 0);
    lv_style_set_pad_hor(&ui_style_header, 16);
    lv_style_set_pad_ver(&ui_style_header, 6);

    /* ================================================================ */
    /* Card  (canvas bg, hairline border, 2 px radius)                  */
    /* ================================================================ */
    lv_style_init(&ui_style_card);
    lv_style_set_bg_color(&ui_style_card, UI_COLOR_CANVAS);
    lv_style_set_bg_opa(&ui_style_card, LV_OPA_COVER);
    lv_style_set_border_color(&ui_style_card, UI_COLOR_HAIRLINE);
    lv_style_set_border_width(&ui_style_card, 1);
    lv_style_set_border_opa(&ui_style_card, LV_OPA_COVER);
    lv_style_set_radius(&ui_style_card, 2);
    lv_style_set_pad_all(&ui_style_card, 8);

    /* ================================================================ */
    /* Primary button  (teal fill, near-black text, 2 px radius)        */
    /* pressed → bright cyan (#00EAFF) per button-primary-active        */
    /* ================================================================ */
    lv_style_init(&ui_style_btn_primary);
    lv_style_set_bg_color(&ui_style_btn_primary, UI_COLOR_PRIMARY);
    lv_style_set_bg_opa(&ui_style_btn_primary, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_btn_primary, UI_COLOR_ON_PRIMARY);
    lv_style_set_radius(&ui_style_btn_primary, 2);
    lv_style_set_pad_hor(&ui_style_btn_primary, 14);
    lv_style_set_pad_ver(&ui_style_btn_primary, 8);
    lv_style_set_border_width(&ui_style_btn_primary, 0);

    /* --- primary pressed --- */
    lv_style_init(&ui_style_btn_primary_pressed);
    lv_style_set_bg_color(&ui_style_btn_primary_pressed, UI_COLOR_PRIMARY_DARK);
    lv_style_set_bg_opa(&ui_style_btn_primary_pressed, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_btn_primary_pressed, UI_COLOR_ON_PRIMARY);

    /* ================================================================ */
    /* Secondary / outline button                                       */
    /* transparent bg, ink text, 2 px primary border                    */
    /* pressed → teal fill + near-black text                            */
    /* ================================================================ */
    lv_style_init(&ui_style_btn_secondary);
    lv_style_set_bg_color(&ui_style_btn_secondary, UI_COLOR_CANVAS);
    lv_style_set_bg_opa(&ui_style_btn_secondary, LV_OPA_TRANSP);
    lv_style_set_text_color(&ui_style_btn_secondary, UI_COLOR_INK);
    lv_style_set_border_color(&ui_style_btn_secondary, UI_COLOR_PRIMARY);
    lv_style_set_border_width(&ui_style_btn_secondary, 2);
    lv_style_set_border_opa(&ui_style_btn_secondary, LV_OPA_COVER);
    lv_style_set_radius(&ui_style_btn_secondary, 2);
    lv_style_set_pad_hor(&ui_style_btn_secondary, 10);
    lv_style_set_pad_ver(&ui_style_btn_secondary, 5);

    /* --- secondary pressed --- */
    lv_style_init(&ui_style_btn_secondary_pressed);
    lv_style_set_bg_color(&ui_style_btn_secondary_pressed, UI_COLOR_PRIMARY);
    lv_style_set_bg_opa(&ui_style_btn_secondary_pressed, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_btn_secondary_pressed, UI_COLOR_ON_PRIMARY);

    /* ================================================================ */
    /* Danger button  (red fill, white text)                            */
    /* pressed → deep red (#650B0B) per error-deep                      */
    /* ================================================================ */
    lv_style_init(&ui_style_btn_danger);
    lv_style_set_bg_color(&ui_style_btn_danger, UI_COLOR_ERROR);
    lv_style_set_bg_opa(&ui_style_btn_danger, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_btn_danger, UI_COLOR_ON_DARK);
    lv_style_set_radius(&ui_style_btn_danger, 2);
    lv_style_set_pad_hor(&ui_style_btn_danger, 24);
    lv_style_set_pad_ver(&ui_style_btn_danger, 10);
    lv_style_set_border_width(&ui_style_btn_danger, 0);

    /* --- danger pressed --- */
    lv_style_init(&ui_style_btn_danger_pressed);
    lv_style_set_bg_color(&ui_style_btn_danger_pressed, UI_COLOR_ERROR_DEEP);
    lv_style_set_bg_opa(&ui_style_btn_danger_pressed, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_btn_danger_pressed, UI_COLOR_ON_DARK);

    /* ================================================================ */
    /* Disabled button (soft-gray bg, ash text)                        */
    /* ================================================================ */
    lv_style_init(&ui_style_btn_disabled);
    lv_style_set_bg_color(&ui_style_btn_disabled, UI_COLOR_SURFACE_SOFT);
    lv_style_set_bg_opa(&ui_style_btn_disabled, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_btn_disabled, UI_COLOR_ASH);

    /* ================================================================ */
    /* Labels                                                           */
    /* ================================================================ */
    lv_style_init(&ui_style_label_value);
    lv_style_set_text_color(&ui_style_label_value, UI_COLOR_INK);
    lv_style_set_text_font(&ui_style_label_value, &lv_font_montserrat_28);

    lv_style_init(&ui_style_label_medium);
    lv_style_set_text_color(&ui_style_label_medium, UI_COLOR_INK);
    lv_style_set_text_font(&ui_style_label_medium, &lv_font_montserrat_20);

    lv_style_init(&ui_style_label_small);
    lv_style_set_text_color(&ui_style_label_small, UI_COLOR_INK);
    lv_style_set_text_font(&ui_style_label_small, &lv_font_montserrat_16);

    lv_style_init(&ui_style_label_caption);
    lv_style_set_text_color(&ui_style_label_caption, UI_COLOR_MUTE);
    lv_style_set_text_font(&ui_style_label_caption, &lv_font_montserrat_12);

    lv_style_init(&ui_style_section_title);
    lv_style_set_text_color(&ui_style_section_title, UI_COLOR_MUTE);
    lv_style_set_text_font(&ui_style_section_title, &lv_font_montserrat_12);

    /* ================================================================ */
    /* Status dots  (8 px circles)                                      */
    /* ================================================================ */
    lv_style_init(&ui_style_online_dot);
    lv_style_set_bg_color(&ui_style_online_dot, UI_COLOR_ONLINE);
    lv_style_set_bg_opa(&ui_style_online_dot, LV_OPA_COVER);
    lv_style_set_radius(&ui_style_online_dot, LV_RADIUS_CIRCLE);
    lv_style_set_width(&ui_style_online_dot, 8);
    lv_style_set_height(&ui_style_online_dot, 8);

    lv_style_init(&ui_style_offline_dot);
    lv_style_set_bg_color(&ui_style_offline_dot, UI_COLOR_OFFLINE);
    lv_style_set_bg_opa(&ui_style_offline_dot, LV_OPA_COVER);
    lv_style_set_radius(&ui_style_offline_dot, LV_RADIUS_CIRCLE);
    lv_style_set_width(&ui_style_offline_dot, 8);
    lv_style_set_height(&ui_style_offline_dot, 8);
}
