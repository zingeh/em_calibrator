/*
 * ui.c — EM Calibrator control screen (LVGL 9.2)
 *
 * Each measurement now shows two lines: TARGET  and  ACTUAL.
 * Distance card:  Target / Actual  (cm)
 * Angle rows:     Target / Actual  (°)
 */

#include "ui.h"
#include "ui_styles.h"
#include "app_config.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui";
static motor_t **g_motors = NULL;

/* Distance */
static lv_obj_t *lbl_dist_tgt, *lbl_dist_act;

/* Base: Yaw / Pitch — target + actual */
static lv_obj_t *lbl_by_tgt, *lbl_by_act, *lbl_bp_tgt, *lbl_bp_act;

/* Tracker: Yaw / Pitch — target + actual */
static lv_obj_t *lbl_ty_tgt, *lbl_ty_act, *lbl_tp_tgt, *lbl_tp_act;

static lv_obj_t *dot_base, *dot_trk;
static lv_obj_t *btn_dm100,*btn_dm10,*btn_dm1,*btn_dp1,*btn_dp10,*btn_dp100;

/* ---- helpers ---- */

static void lbl_s(lv_obj_t *l, const char *t, const lv_style_t *s) {
    lv_label_set_text(l, t); lv_obj_add_style(l, s, 0);
}
static void stp(lv_obj_t *o) {
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
}
static lv_obj_t *btn_pri(lv_obj_t *p, int w, int h, const char *t) {
    lv_obj_t *b = lv_button_create(p); lv_obj_set_size(b,w,h);
    lv_obj_add_style(b, &ui_style_btn_primary, 0);
    lv_obj_add_style(b, &ui_style_btn_primary_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(b, &ui_style_btn_disabled, LV_STATE_DISABLED);
    lv_obj_t *lb=lv_label_create(b); lv_label_set_text(lb,t); lv_obj_center(lb);
    return b;
}
static lv_obj_t *btn_sec(lv_obj_t *p, int w, int h, const char *t) {
    lv_obj_t *b = lv_button_create(p); lv_obj_set_size(b,w,h);
    lv_obj_add_style(b, &ui_style_btn_secondary, 0);
    lv_obj_add_style(b, &ui_style_btn_secondary_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(b, &ui_style_btn_disabled, LV_STATE_DISABLED);
    lv_obj_t *lb=lv_label_create(b); lv_label_set_text(lb,t); lv_obj_center(lb);
    return b;
}
static lv_obj_t *btn_dng(lv_obj_t *p, int w, int h, const char *t) {
    lv_obj_t *b = lv_button_create(p); lv_obj_set_size(b,w,h);
    lv_obj_add_style(b, &ui_style_btn_danger, 0);
    lv_obj_add_style(b, &ui_style_btn_danger_pressed, LV_STATE_PRESSED);
    lv_obj_t *lb=lv_label_create(b); lv_label_set_text(lb,t); lv_obj_center(lb);
    return b;
}
static void dot_set(lv_obj_t *d, bool on) {
    lv_obj_set_style_bg_color(d, on?UI_COLOR_ONLINE:UI_COLOR_OFFLINE, 0);
}

/* ---- callbacks ---- */

#define CB(idx,suf,dlt) \
static void _cb_##idx##_##suf(lv_event_t *e) { \
    (void)e; if(g_motors&&g_motors[idx]) motor_request_relative(g_motors[idx],dlt); }
/* Microsteps (3200/rev): 1cm=3200, 10cm=32000, 100cm=320000, ±90°=800 */
CB(0,m100,-320000) CB(0,m10,-32000) CB(0,m1,-3200)
CB(0,p1,3200)      CB(0,p10,32000)  CB(0,p100,320000)
CB(1,m90,-800) CB(1,p90,800)  CB(2,m90,-800) CB(2,p90,800)
CB(3,m90,-800) CB(3,p90,800)  CB(4,m90,-800) CB(4,p90,800)
#undef CB

static void cb_reset(lv_event_t *e) {
    (void)e; ESP_LOGI(TAG,"RESET ALL — homing");
    for(int i=0;i<5;i++) if(g_motors[i]) motor_request_homing(g_motors[i]);
}

/* ---- timer (200 ms) ---- */

static void timer_cb(lv_timer_t *t) {
    (void)t; if(!g_motors) return; char b[32];

    /* --- Distance --- */
    motor_t *md=g_motors[0];
    if (md&&md->online) {
        int32_t off = md->pos_offset;
        snprintf(b,sizeof(b),"%.1f cm",motor_steps_to_unit(md,md->target_pos+off)/10.0f);
        lv_label_set_text(lbl_dist_tgt,b);
        snprintf(b,sizeof(b),"%.1f cm",motor_steps_to_unit(md,md->current_pos+off)/10.0f);
        lv_label_set_text(lbl_dist_act,b);
    } else {
        lv_label_set_text(lbl_dist_tgt,"--.- cm");
        lv_label_set_text(lbl_dist_act,"--.- cm");
    }

    /* --- Angles --- */
    lv_obj_t *tgt[4]={lbl_by_tgt,lbl_bp_tgt,lbl_ty_tgt,lbl_tp_tgt};
    lv_obj_t *act[4]={lbl_by_act,lbl_bp_act,lbl_ty_act,lbl_tp_act};
    for(int i=0;i<4;i++){motor_t*m=g_motors[i+1];
        if(m&&m->online){
            snprintf(b,sizeof(b),"%+.1f°",motor_steps_to_unit(m,m->target_pos));
            lv_label_set_text(tgt[i],b);
            snprintf(b,sizeof(b),"%+.1f°",motor_steps_to_unit(m,m->current_pos));
            lv_label_set_text(act[i],b);
        } else {
            lv_label_set_text(tgt[i],"---.-°");
            lv_label_set_text(act[i],"---.-°");
        }
    }

    dot_set(dot_base,g_motors[1]&&g_motors[1]->online);
    dot_set(dot_trk, g_motors[3]&&g_motors[3]->online);

    /* --- Limit guards --- */
    if(md&&md->online){
        float mm=motor_steps_to_unit(md,md->target_pos + md->pos_offset);
        bool lo=(mm<=DISTANCE_MIN_MM+1), hi=(mm>=DISTANCE_MAX_MM-1);
        if(lo){lv_obj_add_state(btn_dm100,LV_STATE_DISABLED);
               lv_obj_add_state(btn_dm10,LV_STATE_DISABLED);
               lv_obj_add_state(btn_dm1,LV_STATE_DISABLED);}
        else  {lv_obj_remove_state(btn_dm100,LV_STATE_DISABLED);
               lv_obj_remove_state(btn_dm10,LV_STATE_DISABLED);
               lv_obj_remove_state(btn_dm1,LV_STATE_DISABLED);}
        if(hi){lv_obj_add_state(btn_dp100,LV_STATE_DISABLED);
               lv_obj_add_state(btn_dp10,LV_STATE_DISABLED);
               lv_obj_add_state(btn_dp1,LV_STATE_DISABLED);}
        else  {lv_obj_remove_state(btn_dp100,LV_STATE_DISABLED);
               lv_obj_remove_state(btn_dp10,LV_STATE_DISABLED);
               lv_obj_remove_state(btn_dp1,LV_STATE_DISABLED);}
    }
}

/* ---- angle row: NAME | -90 | +90 ---- */

#define BTN_W_ANG 90
#define BTN_H_ANG 42

static void ang_row2(lv_obj_t *p, const char *nm,
                     lv_obj_t **tgt_out, lv_obj_t **act_out,
                     lv_event_cb_t cm, lv_event_cb_t cp)
{
    lv_obj_t *r=lv_obj_create(p); lv_obj_set_size(r,lv_pct(100),88);
    lv_obj_set_flex_flow(r,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(r,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    stp(r);
    lv_obj_set_style_pad_gap(r,2,0);

    /* Row 1: name + buttons */
    lv_obj_t *rt=lv_obj_create(r); lv_obj_set_size(rt,lv_pct(100),BTN_H_ANG);
    lv_obj_set_flex_flow(rt,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rt,LV_FLEX_ALIGN_SPACE_EVENLY,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    stp(rt);

    lv_obj_t *ln=lv_label_create(rt); lv_label_set_text(ln,nm);
    lv_obj_add_style(ln,&ui_style_label_small,0);

    lv_obj_add_event_cb(btn_sec(rt,BTN_W_ANG,BTN_H_ANG,"-90°"),cm,LV_EVENT_CLICKED,NULL);
    lv_obj_add_event_cb(btn_sec(rt,BTN_W_ANG,BTN_H_ANG,"+90°"),cp,LV_EVENT_CLICKED,NULL);

    /* Row 2: TGT xxx  ACT xxx */
    lv_obj_t *rv=lv_obj_create(r); lv_obj_set_size(rv,lv_pct(100),24);
    lv_obj_set_flex_flow(rv,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rv,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    stp(rv); lv_obj_set_style_pad_gap(rv,12,0);

    lv_obj_t *lt=lv_label_create(rv); lv_label_set_text(lt,"TGT");
    lv_obj_add_style(lt,&ui_style_label_caption,0);
    *tgt_out=lv_label_create(rv); lv_label_set_text(*tgt_out,"---.-°");
    lv_obj_add_style(*tgt_out,&ui_style_label_small,0);

    lv_obj_t *la=lv_label_create(rv); lv_label_set_text(la,"ACT");
    lv_obj_add_style(la,&ui_style_label_caption,0);
    *act_out=lv_label_create(rv); lv_label_set_text(*act_out,"---.-°");
    lv_obj_add_style(*act_out,&ui_style_label_small,0);
}
#undef BTN_W_ANG
#undef BTN_H_ANG

/* ---- body card (BASE / TRACKER) ---- */

static lv_obj_t *body(lv_obj_t *pr, const char *t, lv_obj_t **dot,
                      lv_obj_t **y_tgt,lv_obj_t **y_act,
                      lv_obj_t **p_tgt,lv_obj_t **p_act,
                      lv_event_cb_t ym,lv_event_cb_t yp,
                      lv_event_cb_t pm,lv_event_cb_t pp)
{
    lv_obj_t *c=lv_obj_create(pr); lv_obj_set_size(c,388,LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(c,LV_FLEX_FLOW_COLUMN);
    lv_obj_add_style(c,&ui_style_card,0);
    lv_obj_set_style_pad_all(c,6,0); lv_obj_set_style_pad_gap(c,2,0);
    lv_obj_set_scrollbar_mode(c,LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *tr=lv_obj_create(c); lv_obj_set_size(tr,lv_pct(100),18);
    lv_obj_set_flex_flow(tr,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tr,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    stp(tr);
    lbl_s(lv_label_create(tr),t,&ui_style_section_title);
    *dot=lv_obj_create(tr); lv_obj_set_size(*dot,8,8); dot_set(*dot,false);

    ang_row2(c,"YAW",  y_tgt,y_act,ym,yp);
    ang_row2(c,"PITCH",p_tgt,p_act,pm,pp);
    return c;
}

/* ---- row of 3 distance buttons (each 255×72) ---- */

#define BTN_W_DIST 255
#define BTN_H_DIST 72

static lv_obj_t *btn3(lv_obj_t *p, const char *a,const char *b,const char *c,
                      lv_event_cb_t ca,lv_event_cb_t cb,lv_event_cb_t cc)
{
    lv_obj_t *r=lv_obj_create(p); lv_obj_set_size(r,lv_pct(100),BTN_H_DIST);
    lv_obj_set_flex_flow(r,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r,LV_FLEX_ALIGN_SPACE_EVENLY,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    stp(r);
    lv_obj_add_event_cb(btn_pri(r,BTN_W_DIST,BTN_H_DIST,a),ca,LV_EVENT_CLICKED,NULL);
    lv_obj_add_event_cb(btn_pri(r,BTN_W_DIST,BTN_H_DIST,b),cb,LV_EVENT_CLICKED,NULL);
    lv_obj_add_event_cb(btn_pri(r,BTN_W_DIST,BTN_H_DIST,c),cc,LV_EVENT_CLICKED,NULL);
    return r;
}
#undef BTN_W_DIST
#undef BTN_H_DIST

/* ---- init ---- */

void ui_init(motor_t *motors[5])
{
    g_motors=motors; ui_styles_init();

    lv_obj_t *scr=lv_screen_active();
    lv_obj_set_style_bg_color(scr,UI_COLOR_SURFACE_SOFT,0);
    lv_obj_set_style_pad_all(scr,4,0);
    lv_obj_set_scrollbar_mode(scr,LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *col=lv_obj_create(scr);
    lv_obj_set_size(col,lv_pct(100),lv_pct(100));
    lv_obj_set_flex_flow(col,LV_FLEX_FLOW_COLUMN);
    stp(col); lv_obj_set_style_pad_gap(col,2,0);

    /* — header 24 — */
    lv_obj_t *hd=lv_obj_create(col); lv_obj_set_size(hd,lv_pct(100),24);
    lv_obj_set_flex_flow(hd,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hd,LV_FLEX_ALIGN_SPACE_BETWEEN,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(hd,10,0);
    lv_obj_set_style_border_width(hd,0,0); lv_obj_set_style_radius(hd,0,0);
    lv_obj_set_scrollbar_mode(hd,LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_style(hd,&ui_style_header,0);
    lv_obj_t *ha=lv_label_create(hd); lv_label_set_text(ha,APP_NAME);
    lv_obj_add_style(ha,&ui_style_label_small,0); lv_obj_set_style_text_color(ha,UI_COLOR_ON_DARK,0);
    lv_obj_t *hv=lv_label_create(hd); lv_label_set_text(hv,APP_VERSION);
    lv_obj_add_style(hv,&ui_style_label_caption,0); lv_obj_set_style_text_color(hv,UI_COLOR_ON_DARK,0);

    /* — distance card — */
    lv_obj_t *cd=lv_obj_create(col); lv_obj_set_size(cd,lv_pct(100),LV_SIZE_CONTENT);
    lv_obj_add_style(cd,&ui_style_card,0);
    lv_obj_set_flex_flow(cd,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cd,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cd,8,0); lv_obj_set_style_pad_gap(cd,6,0);
    lv_obj_set_scrollbar_mode(cd,LV_SCROLLBAR_MODE_OFF);
    lbl_s(lv_label_create(cd),"DISTANCE",&ui_style_section_title);

    lv_obj_t *rd=btn3(cd,"-100 cm","-10 cm","-1 cm",_cb_0_m100,_cb_0_m10,_cb_0_m1);

    /* Target / Actual row */
    lv_obj_t *dv=lv_obj_create(cd); lv_obj_set_size(dv,lv_pct(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dv,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dv,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    stp(dv); lv_obj_set_style_pad_gap(dv,12,0);

    lv_obj_t *dt=lv_label_create(dv); lv_label_set_text(dt,"TGT");
    lv_obj_add_style(dt,&ui_style_label_caption,0);
    lbl_dist_tgt=lv_label_create(dv); lv_label_set_text(lbl_dist_tgt,"--.- cm");
    lv_obj_add_style(lbl_dist_tgt,&ui_style_label_value,0);

    lv_obj_t *da=lv_label_create(dv); lv_label_set_text(da,"ACT");
    lv_obj_add_style(da,&ui_style_label_caption,0);
    lbl_dist_act=lv_label_create(dv); lv_label_set_text(lbl_dist_act,"--.- cm");
    lv_obj_add_style(lbl_dist_act,&ui_style_label_medium,0);

    lv_obj_t *ri=btn3(cd,"+1 cm","+10 cm","+100 cm",_cb_0_p1,_cb_0_p10,_cb_0_p100);

    uint32_t nd=lv_obj_get_child_cnt(rd); if(nd>=3){btn_dm100=lv_obj_get_child(rd,0);btn_dm10=lv_obj_get_child(rd,1);btn_dm1=lv_obj_get_child(rd,2);}
    uint32_t ni=lv_obj_get_child_cnt(ri); if(ni>=3){btn_dp1=lv_obj_get_child(ri,0);btn_dp10=lv_obj_get_child(ri,1);btn_dp100=lv_obj_get_child(ri,2);}

    /* — base+tracker row — */
    lv_obj_t *rb=lv_obj_create(col); lv_obj_set_size(rb,lv_pct(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rb,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rb,LV_FLEX_ALIGN_SPACE_EVENLY,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
    stp(rb); lv_obj_set_style_pad_gap(rb,12,0);

    body(rb,"BASE",   &dot_base,
         &lbl_by_tgt,&lbl_by_act, &lbl_bp_tgt,&lbl_bp_act,
         _cb_1_m90,_cb_1_p90, _cb_2_m90,_cb_2_p90);
    body(rb,"TRACKER",&dot_trk,
         &lbl_ty_tgt,&lbl_ty_act, &lbl_tp_tgt,&lbl_tp_act,
         _cb_3_m90,_cb_3_p90, _cb_4_m90,_cb_4_p90);

    /* — RESET ALL — */
    lv_obj_t *res=btn_dng(col,lv_pct(100),52,"RESET ALL");
    lv_obj_add_event_cb(res,cb_reset,LV_EVENT_CLICKED,NULL);

    lv_timer_create(timer_cb,UI_REFRESH_MS,NULL);
    ESP_LOGI(TAG,"UI ready");
}
