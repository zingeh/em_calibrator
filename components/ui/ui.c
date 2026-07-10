/*
 * ui.c — EM Calibrator main control screen (LVGL 9.2)
 *
 * Button hierarchy (ElecholicTech design system):
 *   Distance ±  → primary    (teal fill,       pressed = cyan)
 *   Angle ±90°  → secondary  (clear + teal border, pressed = teal fill)
 *   RESET ALL   → danger     (red fill,         pressed = deep red)
 *   Disabled    → surface-soft bg + ash text
 */

#include "ui.h"
#include "ui_styles.h"
#include "app_config.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui";
static motor_t **g_motors = NULL;

/* Widget refs for timer refresh */
static lv_obj_t *lbl_dist_val;
static lv_obj_t *lbl_base_yaw_val, *lbl_base_pitch_val;
static lv_obj_t *lbl_trk_yaw_val,  *lbl_trk_pitch_val;
static lv_obj_t *dot_base, *dot_trk;
static lv_obj_t *btn_dm100, *btn_dm10, *btn_dm1;
static lv_obj_t *btn_dp1, *btn_dp10, *btn_dp100;

/* ---- Helpers ---- */

static void lbl_set(lv_obj_t *l, const char *t, const lv_style_t *s)
{
    lv_label_set_text(l, t);
    lv_obj_add_style(l, s, 0);
}

static void strip(lv_obj_t *o)
{
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
}

static lv_obj_t *btn_pri(lv_obj_t *p, const char *t)
{
    lv_obj_t *b = lv_button_create(p);
    lv_obj_add_style(b, &ui_style_btn_primary,          0);
    lv_obj_add_style(b, &ui_style_btn_primary_pressed,  LV_STATE_PRESSED);
    lv_obj_add_style(b, &ui_style_btn_disabled,          LV_STATE_DISABLED);
    lv_obj_t *lb = lv_label_create(b);
    lv_label_set_text(lb, t);
    lv_obj_center(lb);
    return b;
}

static lv_obj_t *btn_sec(lv_obj_t *p, const char *t)
{
    lv_obj_t *b = lv_button_create(p);
    lv_obj_add_style(b, &ui_style_btn_secondary,          0);
    lv_obj_add_style(b, &ui_style_btn_secondary_pressed,  LV_STATE_PRESSED);
    lv_obj_add_style(b, &ui_style_btn_disabled,            LV_STATE_DISABLED);
    lv_obj_t *lb = lv_label_create(b);
    lv_label_set_text(lb, t);
    lv_obj_center(lb);
    return b;
}

static lv_obj_t *btn_dng(lv_obj_t *p, const char *t)
{
    lv_obj_t *b = lv_button_create(p);
    lv_obj_add_style(b, &ui_style_btn_danger,          0);
    lv_obj_add_style(b, &ui_style_btn_danger_pressed,  LV_STATE_PRESSED);
    lv_obj_t *lb = lv_label_create(b);
    lv_label_set_text(lb, t);
    lv_obj_center(lb);
    return b;
}

static void dot_set(lv_obj_t *d, bool on)
{
    lv_obj_set_style_bg_color(d, on ? UI_COLOR_ONLINE : UI_COLOR_OFFLINE, 0);
}

/* ---- Button callbacks ---- */

#define CB(idx, suffix, delta) \
static void _cb_##idx##_##suffix(lv_event_t *e) { \
    (void)e; \
    if (g_motors && g_motors[idx]) motor_move_relative(g_motors[idx], delta); \
}
CB(0,m100,-100) CB(0,m10,-10) CB(0,m1,-1)
CB(0,p1,1)      CB(0,p10,10)  CB(0,p100,100)
CB(1,m90,-90)   CB(1,p90,90)
CB(2,m90,-90)   CB(2,p90,90)
CB(3,m90,-90)   CB(3,p90,90)
CB(4,m90,-90)   CB(4,p90,90)
#undef CB

static void cb_reset(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "RESET ALL");
    for (int i = 0; i < 5; i++)
        if (g_motors[i] && g_motors[i]->online)
            motor_move_absolute(g_motors[i], 0);
}

/* ---- Timer refresh (200 ms) — reads CACHED values only ---- */

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!g_motors) return;
    char b[32];

    motor_t *md = g_motors[0];
    if (md && md->online)
        snprintf(b, sizeof(b), "%.1f cm", motor_steps_to_unit(md, md->current_pos) / 10.0f);
    else
        snprintf(b, sizeof(b), "--.- cm");
    lv_label_set_text(lbl_dist_val, b);

    lv_obj_t *als[4] = {lbl_base_yaw_val, lbl_base_pitch_val,
                        lbl_trk_yaw_val,  lbl_trk_pitch_val};
    for (int i = 0; i < 4; i++) {
        motor_t *m = g_motors[i + 1];
        snprintf(b, sizeof(b), "%s",
                 (m && m->online) ? "" : "---.-°");
        if (m && m->online)
            snprintf(b, sizeof(b), "%+.1f°", motor_steps_to_unit(m, m->current_pos));
        lv_label_set_text(als[i], b);
    }

    dot_set(dot_base, g_motors[1] && g_motors[1]->online);
    dot_set(dot_trk,  g_motors[3] && g_motors[3]->online);

    if (md && md->online) {
        float mm = motor_steps_to_unit(md, md->current_pos);
        bool lo = (mm <= DISTANCE_MIN_MM + 1);
        bool hi = (mm >= DISTANCE_MAX_MM - 1);
        if (lo) { lv_obj_add_state(btn_dm100, LV_STATE_DISABLED);
                  lv_obj_add_state(btn_dm10,  LV_STATE_DISABLED);
                  lv_obj_add_state(btn_dm1,   LV_STATE_DISABLED); }
        else    { lv_obj_remove_state(btn_dm100, LV_STATE_DISABLED);
                  lv_obj_remove_state(btn_dm10,  LV_STATE_DISABLED);
                  lv_obj_remove_state(btn_dm1,   LV_STATE_DISABLED); }
        if (hi) { lv_obj_add_state(btn_dp100, LV_STATE_DISABLED);
                  lv_obj_add_state(btn_dp10,  LV_STATE_DISABLED);
                  lv_obj_add_state(btn_dp1,   LV_STATE_DISABLED); }
        else    { lv_obj_remove_state(btn_dp100, LV_STATE_DISABLED);
                  lv_obj_remove_state(btn_dp10,  LV_STATE_DISABLED);
                  lv_obj_remove_state(btn_dp1,   LV_STATE_DISABLED); }
    }
}

/* ---- Angle row ---- */
static void ang_row(lv_obj_t *p, const char *nm, lv_obj_t **val,
                    lv_event_cb_t cm, lv_event_cb_t cp)
{
    lv_obj_t *r = lv_obj_create(p);
    lv_obj_set_size(r, lv_pct(100), 36);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    strip(r);

    lv_obj_t *ln = lv_label_create(r);
    lv_label_set_text(ln, nm);
    lv_obj_add_style(ln, &ui_style_label_small, 0);

    *val = lv_label_create(r);
    lv_label_set_text(*val, "---.-°");
    lv_obj_add_style(*val, &ui_style_label_medium, 0);

    lv_obj_add_event_cb(btn_sec(r, "-90°"), cm, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_sec(r, "+90°"), cp, LV_EVENT_CLICKED, NULL);
}

/* ---- Body card ---- */
static lv_obj_t *body(lv_obj_t *pr, const char *t, lv_obj_t **dot,
                      lv_obj_t **yv, lv_obj_t **pv,
                      lv_event_cb_t ym, lv_event_cb_t yp,
                      lv_event_cb_t pm, lv_event_cb_t pp)
{
    lv_obj_t *c = lv_obj_create(pr);
    lv_obj_set_size(c, 388, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_style(c, &ui_style_card, 0);
    lv_obj_set_style_pad_all(c, 8, 0);
    lv_obj_set_style_pad_gap(c, 6, 0);
    lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *tr = lv_obj_create(c);
    lv_obj_set_size(tr, lv_pct(100), 24);
    lv_obj_set_flex_flow(tr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    strip(tr);
    lbl_set(lv_label_create(tr), t, &ui_style_section_title);
    *dot = lv_obj_create(tr);
    lv_obj_set_size(*dot, 8, 8);
    dot_set(*dot, false);

    ang_row(c, "YAW",   yv, ym, yp);
    ang_row(c, "PITCH", pv, pm, pp);
    return c;
}

/* ---- Row of three primary buttons (distance) ---- */
static lv_obj_t *btn3(lv_obj_t *p, const char *a, const char *b, const char *c,
                      lv_event_cb_t ca, lv_event_cb_t cb, lv_event_cb_t cc)
{
    lv_obj_t *r = lv_obj_create(p);
    lv_obj_set_size(r, lv_pct(100), 44);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    strip(r);
    lv_obj_set_style_pad_gap(r, 8, 0);
    lv_obj_add_event_cb(btn_pri(r, a), ca, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_pri(r, b), cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_pri(r, c), cc, LV_EVENT_CLICKED, NULL);
    return r;
}

/* ---- Entry point ---- */

void ui_init(motor_t *motors[5])
{
    g_motors = motors;
    ui_styles_init();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, UI_COLOR_SURFACE_SOFT, 0);
    lv_obj_set_style_pad_top(scr, 6, 0);
    lv_obj_set_style_pad_bottom(scr, 4, 0);
    lv_obj_set_style_pad_hor(scr, 6, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    /* Main column fills remaining screen area */
    lv_obj_t *col = lv_obj_create(scr);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_style_pad_gap(col, 6, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(col, LV_SCROLLBAR_MODE_OFF);

    /* — Header — */
    lv_obj_t *hd = lv_obj_create(col);
    lv_obj_set_size(hd, lv_pct(100), 36);
    lv_obj_set_flex_flow(hd, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hd, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(hd, 12, 0);
    lv_obj_set_style_border_width(hd, 0, 0);
    lv_obj_set_style_radius(hd, 0, 0);
    lv_obj_set_scrollbar_mode(hd, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_style(hd, &ui_style_header, 0);
    lv_obj_t *ha = lv_label_create(hd);
    lv_label_set_text(ha, APP_NAME);
    lv_obj_add_style(ha, &ui_style_label_small, 0);
    lv_obj_set_style_text_color(ha, UI_COLOR_ON_DARK, 0);
    lv_obj_t *hv = lv_label_create(hd);
    lv_label_set_text(hv, APP_VERSION);
    lv_obj_add_style(hv, &ui_style_label_caption, 0);
    lv_obj_set_style_text_color(hv, UI_COLOR_ON_DARK, 0);

    /* — Distance card — */
    lv_obj_t *cd = lv_obj_create(col);
    lv_obj_set_size(cd, lv_pct(100), 172);
    lv_obj_add_style(cd, &ui_style_card, 0);
    lv_obj_set_flex_flow(cd, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cd, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cd, 8, 0);
    lv_obj_set_style_pad_gap(cd, 6, 0);
    lv_obj_set_scrollbar_mode(cd, LV_SCROLLBAR_MODE_OFF);
    lbl_set(lv_label_create(cd), "DISTANCE", &ui_style_section_title);

    lv_obj_t *rd = btn3(cd, "-100 cm", "-10 cm", "-1 cm",
                         _cb_0_m100, _cb_0_m10, _cb_0_m1);

    lbl_dist_val = lv_label_create(cd);
    lv_label_set_text(lbl_dist_val, "--.- cm");
    lv_obj_add_style(lbl_dist_val, &ui_style_label_value, 0);

    lv_obj_t *ri = btn3(cd, "+1 cm", "+10 cm", "+100 cm",
                         _cb_0_p1, _cb_0_p10, _cb_0_p100);

    uint32_t nd = lv_obj_get_child_cnt(rd);
    if (nd >= 3) { btn_dm100 = lv_obj_get_child(rd, 0);
                   btn_dm10  = lv_obj_get_child(rd, 1);
                   btn_dm1   = lv_obj_get_child(rd, 2); }
    uint32_t ni = lv_obj_get_child_cnt(ri);
    if (ni >= 3) { btn_dp1   = lv_obj_get_child(ri, 0);
                   btn_dp10  = lv_obj_get_child(ri, 1);
                   btn_dp100 = lv_obj_get_child(ri, 2); }

    /* — Base + Tracker row — */
    lv_obj_t *rb = lv_obj_create(col);
    lv_obj_set_size(rb, lv_pct(100), 132);
    lv_obj_set_flex_flow(rb, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rb, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    strip(rb);
    lv_obj_set_style_pad_gap(rb, 12, 0);

    body(rb, "BASE",    &dot_base,    &lbl_base_yaw_val, &lbl_base_pitch_val,
         _cb_1_m90, _cb_1_p90, _cb_2_m90, _cb_2_p90);
    body(rb, "TRACKER", &dot_trk,     &lbl_trk_yaw_val,  &lbl_trk_pitch_val,
         _cb_3_m90, _cb_3_p90, _cb_4_m90, _cb_4_p90);

    /* — RESET ALL — */
    lv_obj_t *res = btn_dng(col, "RESET ALL");
    lv_obj_add_event_cb(res, cb_reset, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(res, lv_pct(100), 44);

    lv_timer_create(timer_cb, UI_REFRESH_MS, NULL);
    ESP_LOGI(TAG, "UI ready");
}
