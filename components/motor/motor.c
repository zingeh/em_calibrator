/*
 * motor.c — thin wrapper: our API → Emm_V5 binary protocol
 *
 * Emm_V5 builds command frames; we send via RS485 and receive responses.
 * All Emm_V5 send functions end with checksum byte 0x6B.
 * Response format: [addr][cmd][data...][0x6B]
 */

#include "motor.h"
#include "Emm_V5.h"
#include "rs485.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "motor";

static inline int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{ return (v < lo) ? lo : (v > hi) ? hi : v; }

/* ---- recv helper ---- */

static esp_err_t emm_recv(uint8_t addr, size_t *n, uint8_t *rx, size_t max,
                           int timeout_ms)
{
    esp_err_t ret = rs485_raw_recv(rx, max, n, timeout_ms);
    if (ret != ESP_OK) return ret;

    /* Validate: [addr]...[0x6B] */
    if (*n >= 2 && rx[0] == addr && rx[*n-1] == 0x6B)
        return ESP_OK;
    return ESP_ERR_INVALID_RESPONSE;
}

/* ---- QPos arm ---- */

/* QPos_Control (0xFC, position-difference mode) does not carry speed
 * inline, and this motor requires Set_QPos_Params (0xF1) to be re-armed
 * before EVERY QPos_Control — without a fresh Set_QPos_Params the
 * QPos_Control is silently ignored (only the first move after power-up
 * works).  Send it every time with the current speed, and consume the
 * motor's ACK so it can't collide with the following QPos_Control on
 * the half-duplex RS485 bus. */
static void qpos_arm(motor_t *m)
{
    rs485_flush_rx();
    Emm_V5_Set_QPos_Params(m->id, m->speed, m->accel, 1, false);
    {
        uint8_t rx[16]; size_t n;
        (void)emm_recv(m->id, &n, rx, sizeof(rx), 50);
    }
    rs485_flush_rx();
    m->qpos_speed = m->speed;
}

/* ---- init ---- */

esp_err_t motor_init(motor_t *m, uint8_t id, motor_type_t type,
                     const char *name, int32_t max_s, int32_t min_s,
                     float spu, uint16_t speed, uint16_t speed_max)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    memset(m, 0, sizeof(*m));
    m->id = id; m->type = type;
    strncpy(m->name, name, sizeof(m->name)-1);
    m->max_steps = max_s; m->min_steps = min_s;
    m->steps_per_unit = spu;
    m->speed          = speed;
    m->speed_default  = speed;
    m->speed_max      = speed_max;
    m->qpos_speed     = 0;          /* not yet configured on motor */
    m->speed_override = false;
    m->accel          = 50;
    m->target_pos     = 0;
    m->current_pos = 0;
    return ESP_OK;
}

/* ---- probe ---- */

esp_err_t motor_probe(motor_t *m)
{
    if (!m) return ESP_ERR_INVALID_ARG;

    rs485_flush_rx();

    /* Emm_V5_Read_Sys_Params (S_FLAG = 19 → cmd 0x3A) → sends [addr 3A 6B] */
    Emm_V5_Read_Sys_Params(m->id, S_FLAG);

    uint8_t rx[16]; size_t n;
    esp_err_t ret = emm_recv(m->id, &n, rx, sizeof(rx), 50);

    if (ret == ESP_OK && n >= 4) {
        /* Response: [addr][0x3A][flag1][flag0][0x6B]  or  [addr][0x3A][flags][0x6B] */
        m->online = true;
        ESP_LOGI(TAG, "%s (id=%d) online (%d bytes)", m->name, m->id, (int)n);
        motor_enable(m);
        return ESP_OK;
    }

    if (n > 0) {
        char d[64]={0}; int o=0;
        for(int i=0;i<(int)n&&o<55;i++) o+=snprintf(d+o,sizeof(d)-o,"%02X ",rx[i]);
        ESP_LOGI(TAG, "probe RX: %s", d);
    }
    ESP_LOGW(TAG, "%s (id=%d) offline — %d bytes", m->name, m->id, (int)n);
    return ESP_ERR_TIMEOUT;
}

/* ---- enable ---- */

esp_err_t motor_enable(motor_t *m)
{
    if (!m || !m->online) return ESP_ERR_INVALID_STATE;
    Emm_V5_En_Control(m->id, true, false);  /* state=true, snF=now */
    vTaskDelay(pdMS_TO_TICKS(50));
    m->enabled = true;
    ESP_LOGI(TAG, "%s enabled", m->name);
    return ESP_OK;
}

/* ---- absolute move ---- */

esp_err_t motor_move_absolute(motor_t *m, int32_t steps)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    if (!m->online) { ESP_LOGW(TAG, "%s offline — move ignored", m->name); return ESP_ERR_INVALID_STATE; }

    steps = clamp_i32(steps, m->min_steps, m->max_steps);

    if (!m->enabled) {
        ESP_LOGW(TAG, "%s not enabled — enabling now", m->name);
        motor_enable(m);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* QPos_Control clk is an ABSOLUTE target position (verified on HW:
     * sending the same clk twice does not move the second time, and
     * Pos_Control raF=0 ACCUMULATES into the motor's target register).
     * So send the absolute position directly. */
    if (steps == m->current_pos) {
        ESP_LOGI(TAG, "%s abs skipped — already at %ld", m->name, (long)steps);
        m->target_pos = steps;
        return ESP_OK;
    }

    ESP_LOGI(TAG, "%s → abs %ld steps (cur %ld, %u RPM)",
             m->name, (long)steps, (long)m->current_pos, m->speed);

    qpos_arm(m);
    Emm_V5_QPos_Control(m->id, steps);

    m->target_pos = steps;
    m->current_pos = steps;  /* optimistic — corrected by next read */
    m->moving = true;
    return ESP_OK;
}

/* ---- relative move (QPos — FC 0xFC, signed int32 delta) ---- */

esp_err_t motor_move_relative(motor_t *m, int32_t delta)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    if (!m->online) { ESP_LOGW(TAG, "%s offline — rel ignored", m->name); return ESP_ERR_INVALID_STATE; }

    if (!m->enabled) {
        motor_enable(m);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    /* Use target_pos (last known good position), not current_pos (may be stale).
     * current_pos is only updated by motor_read_position, which can fail. */
    int32_t clamped = clamp_i32(m->target_pos + delta, m->min_steps, m->max_steps);
    delta = clamped - m->target_pos;

    if (delta == 0) {
        ESP_LOGI(TAG, "%s rel skipped — already at limit", m->name);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "%s rel %+ld → abs %ld (cur %ld, %u RPM)",
             m->name, (long)delta, (long)clamped, (long)m->current_pos, m->speed);

    qpos_arm(m);
    Emm_V5_QPos_Control(m->id, clamped);

    m->target_pos = clamped;
    m->current_pos = clamped;  /* optimistic — corrected by next read */
    m->moving = true;
    return ESP_OK;
}

/* ---- async (LVGL thread — set pending_delta, poll task executes) ---- */

esp_err_t motor_request_relative(motor_t *m, int32_t delta)
{ if (!m) return ESP_ERR_INVALID_ARG; m->pending_delta = delta; return ESP_OK; }

esp_err_t motor_request_absolute(motor_t *m, int32_t steps, uint16_t rpm)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    m->pending_abs = steps;
    m->pending_abs_flag = true;
    if (rpm > 0) { m->speed = rpm; m->speed_override = true; }
    return ESP_OK;
}

/* ---- stop ---- */

esp_err_t motor_stop(motor_t *m)
{
    if (!m || !m->online) return ESP_ERR_INVALID_STATE;
    Emm_V5_Stop_Now(m->id, false);
    m->moving = false;
    ESP_LOGI(TAG, "%s stop", m->name);
    return ESP_OK;
}

/* ---- zero ---- */

esp_err_t motor_set_zero(motor_t *m)
{
    if (!m || !m->online) return ESP_ERR_INVALID_STATE;
    Emm_V5_Reset_CurPos_To_Zero(m->id);
    m->current_pos = 0;
    m->target_pos = 0;
    ESP_LOGI(TAG, "%s zeroed", m->name);
    return ESP_OK;
}

/* ---- calibrate ---- */

esp_err_t motor_calibrate(motor_t *m)
{
    if (!m || !m->online) return ESP_ERR_INVALID_STATE;
    Emm_V5_Trig_Encoder_Cal(m->id);
    ESP_LOGI(TAG, "%s calibrated", m->name);
    return ESP_OK;
}

/* ---- read position (S_CPOS = 15 → cmd 0x36) ---- */

esp_err_t motor_read_position(motor_t *m)
{
    if (!m || !m->online) return ESP_ERR_INVALID_STATE;

    rs485_flush_rx();
    Emm_V5_Read_Sys_Params(m->id, S_CPOS);  /* [addr][0x36][0x6B] */

    /* Give the motor a moment to respond */
    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t rx[16]; size_t n;
    esp_err_t ret = emm_recv(m->id, &n, rx, sizeof(rx), 30);
    if (ret != ESP_OK) {
        /* Don't mark offline yet — one bad read could be bus contention */
        if (n > 0) {
            char d[64]={0}; int o=0;
            for(int i=0;i<(int)n&&o<55;i++) o+=snprintf(d+o,sizeof(d)-o,"%02X ",rx[i]);
            ESP_LOGW(TAG, "%s pos read fail — got %d B: %s", m->name, (int)n, d);
        } else {
            ESP_LOGW(TAG, "%s pos read fail — timeout", m->name);
        }
        return ret;
    }

    /* Dump raw response for diagnosis */
    {
        char d[64]={0}; int o=0;
        for(int i=0;i<(int)n&&o<55;i++) o+=snprintf(d+o,sizeof(d)-o,"%02X ",rx[i]);
        ESP_LOGI(TAG, "%s pos RX %d B: %s", m->name, (int)n, d);
    }

    /* [addr][0x36][sign][B3][B2][B1][B0][0x6B]
     * sign: 0x00=+, 0x01=-
     * position: 32-bit encoder ticks, big-endian, 65536 ticks/rev
     * Convert to microsteps: 3200 µsteps/rev ÷ 65536 ticks/rev */
    if (n >= 8 && rx[1] == 0x36 && rx[n-1] == 0x6B) {
        uint32_t raw = ((uint32_t)rx[3]<<24)|((uint32_t)rx[4]<<16)
                      |((uint32_t)rx[5]<<8) |(uint32_t)rx[6];
        int32_t enc_pos = (rx[2] == 0x01) ? -(int32_t)raw : (int32_t)raw;
        /* encoder ticks → microsteps */
        m->current_pos = (int32_t)(((int64_t)enc_pos * 3200 + 32768) / 65536);

        /* Clear "moving" once the motor has arrived at its target
         * (within tolerance). LINEAR: 2 mm; ROTARY: 1°. */
        if (m->moving) {
            int32_t tol = (m->type == MOTOR_LINEAR)
                        ? (int32_t)(m->steps_per_unit * 2.0f)
                        : (int32_t)(m->steps_per_unit * 1.0f);
            if (tol < 10) tol = 10;
            int32_t err = m->target_pos - m->current_pos;
            if (err < 0) err = -err;
            if (err <= tol) m->moving = false;
        }
    } else {
        char d[64]={0}; int o=0;
        for(int i=0;i<(int)n&&o<55;i++) o+=snprintf(d+o,sizeof(d)-o,"%02X ",rx[i]);
        ESP_LOGW(TAG, "%s pos parse fail — %d B: %s", m->name, (int)n, d);
    }
    return ESP_OK;
}

/* ---- background poll task ---- */

static motor_t **poll_motors  = NULL;
static int       poll_count   = 0;
static int       poll_interval = 200;

static void motor_poll_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Poll task — %d motors @ %d ms", poll_count, poll_interval);
    while (1) {
        bool did_move = false;
        for (int i = 0; i < poll_count; i++) {
            motor_t *m = poll_motors[i];
            if (!m || !m->online) continue;
            /* This motor ignores new move commands while it is still
             * executing — wait until it has arrived (moving cleared by
             * motor_read_position) before sending the next one. */
            if (m->moving) continue;

            if (m->pending_abs_flag) {
                int32_t target = m->pending_abs;
                m->pending_abs = 0; m->pending_abs_flag = false;
                ESP_LOGI(TAG, "poll: %s abs→%ld", m->name, (long)target);
                motor_move_absolute(m, target);
                if (m->speed_override) { m->speed = m->speed_default; m->speed_override = false; }
                did_move = true;
                vTaskDelay(pdMS_TO_TICKS(20));
            } else if (m->pending_delta != 0) {
                int32_t d = m->pending_delta; m->pending_delta = 0;
                ESP_LOGI(TAG, "poll: %s delta=%ld", m->name, (long)d);
                motor_move_relative(m, d);
                if (m->speed_override) { m->speed = m->speed_default; m->speed_override = false; }
                did_move = true;
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
        /* If we just sent a move command, let the motor process it
         * before sending more frames.  Otherwise read immediately. */
        if (did_move)
            vTaskDelay(pdMS_TO_TICKS(100));
        else
            vTaskDelay(pdMS_TO_TICKS(10));

        for (int i = 0; i < poll_count; i++) {
            motor_t *m = poll_motors[i];
            if (m && m->online) motor_read_position(m);
        }
        vTaskDelay(pdMS_TO_TICKS(poll_interval));
    }
}

esp_err_t motor_start_poll_task(motor_t **motors, int count, int interval_ms)
{
    poll_motors = motors; poll_count = count; poll_interval = interval_ms;
    if (xTaskCreatePinnedToCore(motor_poll_task, "motor_poll",
                                 4*1024, NULL, 1, NULL,
                                 tskNO_AFFINITY) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create motor poll task");
        return ESP_FAIL;
    }
    return ESP_OK;
}
