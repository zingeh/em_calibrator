/*
 * ui_styles.h — ElecholicTech design-system tokens as LVGL styles
 *
 *   Primary  #0095a3    Primary-dark  #00eaff
 *   Ink      #010A11    Canvas        #ffffff
 *   Surface-dark  #010A11    Surface-soft  #f7f7f7
 *   Hairline      #cccccc    Mute          #757575
 *   Ash           #a7a7a7    Error         #e52020
 *   On-dark       #ffffff    On-primary    #010A11
 *   Online        #3f8500    Offline       #e52020
 *
 *   2 px radius on every interactive element.  No shadows.
 */

#pragma once

#include "lvgl.h"

/* ---- Design-token colours ---- */
#define UI_COLOR_PRIMARY        lv_color_hex(0x0095A3)
#define UI_COLOR_PRIMARY_DARK   lv_color_hex(0x00EAFF)
#define UI_COLOR_INK            lv_color_hex(0x010A11)
#define UI_COLOR_CANVAS         lv_color_hex(0xFFFFFF)
#define UI_COLOR_SURFACE_DARK   lv_color_hex(0x010A11)
#define UI_COLOR_SURFACE_SOFT   lv_color_hex(0xF7F7F7)
#define UI_COLOR_HAIRLINE       lv_color_hex(0xCCCCCC)
#define UI_COLOR_ON_DARK        lv_color_hex(0xFFFFFF)
#define UI_COLOR_ON_PRIMARY     lv_color_hex(0x010A11)
#define UI_COLOR_ERROR          lv_color_hex(0xE52020)
#define UI_COLOR_ERROR_DEEP     lv_color_hex(0x650B0B)
#define UI_COLOR_MUTE           lv_color_hex(0x757575)
#define UI_COLOR_ASH            lv_color_hex(0xA7A7A7)
#define UI_COLOR_ONLINE         lv_color_hex(0x3F8500)
#define UI_COLOR_OFFLINE        lv_color_hex(0xE52020)

/* ---- Styles (default + pressed) ---- */
extern lv_style_t ui_style_header;
extern lv_style_t ui_style_card;
extern lv_style_t ui_style_btn_primary;
extern lv_style_t ui_style_btn_primary_pressed;
extern lv_style_t ui_style_btn_secondary;
extern lv_style_t ui_style_btn_secondary_pressed;
extern lv_style_t ui_style_btn_danger;
extern lv_style_t ui_style_btn_danger_pressed;
extern lv_style_t ui_style_btn_disabled;
extern lv_style_t ui_style_label_value;        /* 28 px bold ink       */
extern lv_style_t ui_style_label_medium;       /* 20 px bold ink       */
extern lv_style_t ui_style_label_small;        /* 16 px bold ink       */
extern lv_style_t ui_style_label_caption;      /* 12 px regular mute   */
extern lv_style_t ui_style_section_title;      /* 12 px bold mute      */
extern lv_style_t ui_style_online_dot;
extern lv_style_t ui_style_offline_dot;

void ui_styles_init(void);
