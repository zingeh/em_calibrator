/*
 * main.c — EM Calibrator application entry point
 *
 * Initialises the Waveshare 4.3" LCD, RS485 MODBUS bus, 5 stepper
 * motors, and the LVGL control UI.  Peripheral failures (touch, IO
 * expander, individual motors) are non-fatal — the UI stays usable.
 */

#include "app_config.h"
#include "rgb_lcd_port.h"
#include "gt911.h"
#include "io_extension.h"
#include "rs485.h"
#include "motor.h"
#include "ui.h"
#include "lvgl_port.h"
#include "commander.h"
#include "esp_log.h"

static const char *TAG = "main";

/* ---- I2C scanner (for debug) ---- */
static void i2c_scan(i2c_master_bus_handle_t bus)
{
    ESP_LOGI(TAG, "=== I2C Bus Scan ===");
    int found = 0;
    for (uint8_t a = 1; a < 127; a++) {
        if (i2c_master_probe(bus, a, 50) == ESP_OK) {
            ESP_LOGI(TAG, "  Found 0x%02X", a);
            found++;
        }
    }
    ESP_LOGI(TAG, "=== %d device(s) found ===", found);
}

void app_main(void)
{
    /* ---------- I2C & peripherals (non-fatal) ---------- */
    DEV_I2C_Init();
    i2c_scan(DEV_I2C_Get_Bus_Device());
    IO_EXTENSION_Init();

    /* ---------- Touch (non-fatal) ---------- */
    esp_lcd_touch_handle_t tp = touch_gt911_init(DEV_I2C_Get_Bus_Device());

    /* ---------- LCD (must succeed) ---------- */
    esp_lcd_panel_handle_t panel = waveshare_esp32_s3_rgb_lcd_init();

    /* ---------- RS485 bus ---------- */
    ESP_ERROR_CHECK(rs485_init(RS485_UART_PORT,
                                     RS485_TX_PIN, RS485_RX_PIN,
                                     RS485_DE_PIN, RS485_BAUD));

    /* ---------- Motor array ---------- */
    static motor_t *motors[5];
    static motor_t m_buf[5];
    memset(m_buf, 0, sizeof(m_buf));

    /* Steps-per-unit:
     *   distance: STEPS_PER_REV / LEAD_SCREW_PITCH_MM * 10 (for cm: 3200/10*10=3200)
     *   angle:    STEPS_PER_REV / 360.0                                  */
    float spu_dist = (float)MOTOR_STEPS_PER_REV / LEAD_SCREW_PITCH_MM;  /* steps/mm */
    float spu_ang  = (float)MOTOR_STEPS_PER_REV / 360.0f;               /* steps/deg */

    motor_init(&m_buf[0], MOTOR_ID_DISTANCE,   MOTOR_LINEAR,
               "Distance",  (int32_t)(DISTANCE_MAX_MM * spu_dist),
               (int32_t)(DISTANCE_MIN_MM * spu_dist),
               spu_dist, MOTOR_SPEED_DISTANCE);
    m_buf[0].pos_offset = (int32_t)(100.0f * spu_dist);  /* 10 cm physical offset */
    motors[0] = &m_buf[0];

    motor_init(&m_buf[1], MOTOR_ID_BASE_YAW,   MOTOR_ROTARY,
               "Base Yaw",   (int32_t)(YAW_MAX_DEG   * spu_ang),
               (int32_t)(YAW_MIN_DEG   * spu_ang),
               spu_ang, MOTOR_SPEED_ANGLE);
    motors[1] = &m_buf[1];

    motor_init(&m_buf[2], MOTOR_ID_BASE_PITCH, MOTOR_ROTARY,
               "Base Pitch", (int32_t)(PITCH_MAX_DEG * spu_ang),
               (int32_t)(PITCH_MIN_DEG * spu_ang),
               spu_ang, MOTOR_SPEED_ANGLE);
    motors[2] = &m_buf[2];

    motor_init(&m_buf[3], MOTOR_ID_TRACK_YAW,  MOTOR_ROTARY,
               "Track Yaw",  (int32_t)(YAW_MAX_DEG   * spu_ang),
               (int32_t)(YAW_MIN_DEG   * spu_ang),
               spu_ang, MOTOR_SPEED_ANGLE);
    motors[3] = &m_buf[3];

    motor_init(&m_buf[4], MOTOR_ID_TRACK_PITCH,MOTOR_ROTARY,
               "Track Pitch",(int32_t)(PITCH_MAX_DEG * spu_ang),
               (int32_t)(PITCH_MIN_DEG * spu_ang),
               spu_ang, MOTOR_SPEED_ANGLE);
    motors[4] = &m_buf[4];

    /* ---------- LVGL + UI ---------- */
    lvgl_port_init(panel, tp);

    /* Create UI, then delay to let LVGL task render first frame */
    if (lvgl_port_lock(-1)) {
        ui_init(motors);
        lvgl_port_unlock();
    }

    /* Wait for LVGL task to render the first frame (task initial delay ≤500ms) */
    vTaskDelay(pdMS_TO_TICKS(800));

    /* Turn on backlight */
    waveshare_rgb_lcd_bl_on();

    /* Probe motors, then start background position polling */
    for (int i = 0; i < 5; i++)
        motor_probe(motors[i]);

    motor_start_poll_task(motors, 5, 100);  /* poll every 100ms for snappy button response */

    /* Start G-code commander (USB CDC + WiFi TCP:8888) */
    commander_cfg_t cmd_cfg = { .motors = motors, .motor_count = 5 };
    commander_init(&cmd_cfg);

    ESP_LOGI(TAG, "Init complete — EM Calibrator ready");
}
