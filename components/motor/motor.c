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

/* ---- init ---- */

esp_err_t motor_init(motor_t *m, uint8_t id, motor_type_t type,
                     const char *name, int32_t max_s, int32_t min_s,
                     float spu, uint16_t speed)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    memset(m, 0, sizeof(*m));
    m->id = id; m->type = type;
    strncpy(m->name, name, sizeof(m->name)-1);
    m->max_steps = max_s; m->min_steps = min_s;
    m->steps_per_unit = spu;
    m->speed = speed;
    m->accel = 50;
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
    m->enabled = true;
    ESP_LOGI(TAG, "%s enabled", m->name);
    return ESP_OK;
}

/* ---- absolute move (position mode) ---- */

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

    ESP_LOGI(TAG, "%s → %ld steps (%u RPM, acc=%d)",
             m->name, (long)steps, m->speed, m->accel);

    /* raF=2: absolute move relative to current real-time position */
    Emm_V5_Pos_Control(m->id, 0, m->speed, m->accel, (uint32_t)steps, 2, false);

    m->target_pos = steps;
    m->moving = true;
    return ESP_OK;
}

/* ---- relative move ---- */

esp_err_t motor_move_relative(motor_t *m, int32_t delta)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    int32_t t = clamp_i32(m->current_pos + delta, m->min_steps, m->max_steps);
    return motor_move_absolute(m, t);
}

/* ---- async (LVGL thread — set pending_delta, poll task executes) ---- */

esp_err_t motor_request_relative(motor_t *m, int32_t delta)
{ if (!m) return ESP_ERR_INVALID_ARG; m->pending_delta = delta; return ESP_OK; }

esp_err_t motor_request_absolute(motor_t *m, int32_t steps)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    steps = clamp_i32(steps, m->min_steps, m->max_steps);
    m->pending_delta = steps - m->current_pos;
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

    uint8_t rx[16]; size_t n;
    esp_err_t ret = emm_recv(m->id, &n, rx, sizeof(rx), 30);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "%s pos read fail", m->name);
        m->online = false;
        return ret;
    }

    /* [addr][0x36][B3][B2][B1][B0][0x6B] */
    if (n >= 7 && rx[1] == 0x36) {
        m->current_pos = ((int32_t)rx[2]<<24)|((int32_t)rx[3]<<16)
                        |((int32_t)rx[4]<<8)|(int32_t)rx[5];
    }
    return ESP_OK;
}

/* ---- homing stub ---- */

esp_err_t motor_homing(motor_t *m)
{
    if (!m || !m->online) return ESP_ERR_INVALID_STATE;
    ESP_LOGW(TAG, "%s homing — placeholder", m->name);
    return motor_set_zero(m);
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
        for (int i = 0; i < poll_count; i++) {
            motor_t *m = poll_motors[i];
            if (m && m->online && m->pending_delta != 0) {
                int32_t d = m->pending_delta; m->pending_delta = 0;
                ESP_LOGI(TAG, "poll: %s exec delta=%ld", m->name, (long)d);
                motor_move_relative(m, d);
            }
        }
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
