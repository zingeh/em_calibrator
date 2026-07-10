/*
 * motor.c — Stepper motor driver using ZDT MODBUS RTU
 *
 * MODBUS register map (ZDT closed-loop driver):
 *   0x0006  WO   Encoder calibration (write 1)
 *   0x000A  WO   Position clear / set zero (write 1)
 *   0x0030  RO   Real-time pulse count (6 bytes)
 *   0x0385  RW   Running speed (RPM, uint16)
 *   0x0386  RW   Target position low 16
 *   0x0387  RW   Target position high 16
 *   0x038B  RO   Position-arrival flag
 *   0x0064  RO   Alarm code
 */

#include "motor.h"
#include "modbus_485.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "motor";

/* ---- Utility: clamp ---- */
static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ---- Init ---- */

esp_err_t motor_init(motor_t *m, uint8_t id, motor_type_t type,
                     const char *name, int32_t max_steps, int32_t min_steps,
                     float steps_per_unit, uint16_t speed)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    memset(m, 0, sizeof(*m));
    m->id   = id;
    m->type = type;
    strncpy(m->name, name, sizeof(m->name) - 1);
    m->max_steps      = max_steps;
    m->min_steps      = min_steps;
    m->steps_per_unit = steps_per_unit;
    m->speed          = speed;
    m->online         = false;
    m->moving         = false;
    return ESP_OK;
}

/* ---- Probe ---- */

esp_err_t motor_probe(motor_t *m)
{
    /* Read alarm register — a successful read means motor is online */
    uint8_t rx[2];
    esp_err_t ret = modbus_485_transaction(m->id, MODBUS_FC_READ_INPUT,
                                             REG_ALARM, 1, rx, 2, 100);
    m->online = (ret == ESP_OK);
    if (m->online) {
        ESP_LOGI(TAG, "%s (id=%d) online — alarm=0x%02X%02X",
                 m->name, m->id, rx[0], rx[1]);
    } else {
        ESP_LOGW(TAG, "%s (id=%d) offline — err 0x%x", m->name, m->id, ret);
    }
    return ret;
}

/* ---- Motion ---- */

esp_err_t motor_move_absolute(motor_t *m, int32_t steps)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    if (!m->online) return ESP_ERR_INVALID_STATE;

    steps = clamp_i32(steps, m->min_steps, m->max_steps);

    /* Set speed first */
    esp_err_t ret = modbus_485_transaction(m->id, MODBUS_FC_WRITE_SINGLE,
                                            REG_SPEED, m->speed, NULL, 0, -1);
    if (ret != ESP_OK) { m->online = false; return ret; }

    /* Write target position as two 16-bit registers */
    uint16_t lo = (uint16_t)(steps & 0xFFFF);
    uint16_t hi = (uint16_t)((steps >> 16) & 0xFFFF);

    /* Write low word */
    ret = modbus_485_transaction(m->id, MODBUS_FC_WRITE_SINGLE,
                                  REG_TARGET_POS_LO, lo, NULL, 0, -1);
    if (ret != ESP_OK) { m->online = false; return ret; }

    /* Write high word */
    ret = modbus_485_transaction(m->id, MODBUS_FC_WRITE_SINGLE,
                                  REG_TARGET_POS_HI, hi, NULL, 0, -1);
    if (ret != ESP_OK) { m->online = false; return ret; }

    m->target_pos = steps;
    m->moving = true;

    ESP_LOGI(TAG, "%s → %ld steps", m->name, (long)steps);
    return ESP_OK;
}

esp_err_t motor_move_relative(motor_t *m, int32_t delta)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    int32_t target = clamp_i32(m->current_pos + delta, m->min_steps, m->max_steps);
    return motor_move_absolute(m, target);
}

esp_err_t motor_stop(motor_t *m)
{
    if (!m || !m->online) return ESP_ERR_INVALID_STATE;
    /* Write zero speed to stop? Actually just move to current position.
     * ZDT doesn't have a dedicated stop — write current pos as target. */
    ESP_LOGW(TAG, "%s stop — moving to current position %ld",
             m->name, (long)m->current_pos);
    return motor_move_absolute(m, m->current_pos);
}

/* ---- Zero / Calibrate ---- */

esp_err_t motor_set_zero(motor_t *m)
{
    if (!m || !m->online) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = modbus_485_transaction(m->id, MODBUS_FC_WRITE_SINGLE,
                                            REG_CLEAR_POS, 0x0001, NULL, 0, 50);
    if (ret == ESP_OK) {
        m->current_pos = 0;
        m->target_pos = 0;
        ESP_LOGI(TAG, "%s position zeroed", m->name);
    } else {
        m->online = false;
    }
    return ret;
}

esp_err_t motor_calibrate(motor_t *m)
{
    if (!m || !m->online) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = modbus_485_transaction(m->id, MODBUS_FC_WRITE_SINGLE,
                                            REG_CALIBRATE, 0x0001, NULL, 0, 50);
    if (ret != ESP_OK) m->online = false;
    else ESP_LOGI(TAG, "%s encoder calibrated", m->name);
    return ret;
}

/* ---- Read position ---- */

esp_err_t motor_read_position(motor_t *m)
{
    if (!m || !m->online) return ESP_ERR_INVALID_STATE;

    uint8_t rx[6];
    esp_err_t ret = modbus_485_transaction(m->id, MODBUS_FC_READ_INPUT,
                                            REG_CURRENT_POS, 3, rx, 6, 100);
    if (ret != ESP_OK) {
        m->online = false;
        ESP_LOGW(TAG, "%s read position failed — marking offline", m->name);
        return ret;
    }

    /* ZDT format: [sign_byte][pos_byte1][pos_byte2][pos_byte3][pos_byte4][??]
     * sign: 0x00 = positive, 0x01 = negative
     * position: 32-bit signed, MSB first */
    int32_t pos = 0;
    if (rx[0] == 0x01) {
        /* Negative — two's complement */
        pos = (int32_t)((rx[1] << 24) | (rx[2] << 16) | (rx[3] << 8) | rx[4]);
        pos = -pos;
    } else {
        pos = (int32_t)((rx[1] << 24) | (rx[2] << 16) | (rx[3] << 8) | rx[4]);
    }
    m->current_pos = pos;

    /* Check arrival flag */
    uint8_t arr[2];
    ret = modbus_485_transaction(m->id, MODBUS_FC_READ_INPUT,
                                  REG_POS_ARRIVED, 1, arr, 2, 50);
    m->moving = (ret == ESP_OK) ? (arr[1] == 0) : true;

    return ESP_OK;
}

/* ---- Homing stub ---- */

esp_err_t motor_homing(motor_t *m)
{
    if (!m || !m->online) return ESP_ERR_INVALID_STATE;
    /*
     * TODO: Insert application-specific homing logic here.
     * e.g. move toward limit switch until triggered, then back off.
     * For now, just set current position to zero.
     */
    ESP_LOGW(TAG, "%s homing — using motor_set_zero() as placeholder. "
             "Implement limit-switch logic here.", m->name);
    return motor_set_zero(m);
}

/* ---- Background position polling task ---- */

static motor_t     **poll_motors  = NULL;
static int           poll_count   = 0;
static int           poll_interval = 200;

static void motor_poll_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Poll task started — %d motors, %d ms interval", poll_count, poll_interval);

    while (1) {
        for (int i = 0; i < poll_count; i++) {
            motor_t *m = poll_motors[i];
            if (m && m->online)
                motor_read_position(m);
        }
        vTaskDelay(pdMS_TO_TICKS(poll_interval));
    }
}

esp_err_t motor_start_poll_task(motor_t **motors, int count, int interval_ms)
{
    poll_motors   = motors;
    poll_count    = count;
    poll_interval = interval_ms;

    if (xTaskCreatePinnedToCore(motor_poll_task, "motor_poll",
                                 3 * 1024, NULL, 1, NULL, tskNO_AFFINITY) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create motor poll task");
        return ESP_FAIL;
    }
    return ESP_OK;
}
