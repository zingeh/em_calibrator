/*
 * motor.h — ZDT closed-loop stepper motor driver (Emm_V5 binary protocol)
 *
 * Protocol format:  [addr][cmd_hi][cmd_lo]...[0x6B]
 *   addr   = motor slave address (1-255, 0 = broadcast)
 *   0x6B   = fixed checksum byte
 *
 * All frames are built by functions in Emm_V5.c.
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum { MOTOR_LINEAR = 0, MOTOR_ROTARY = 1 } motor_type_t;

typedef struct motor {
    uint8_t      id;
    motor_type_t type;
    char         name[16];

    int32_t      max_steps;        /* software limit            */
    int32_t      min_steps;
    float        steps_per_unit;   /* steps per mm or degree    */
    uint16_t     speed;            /* RPM                       */
    uint8_t      accel;            /* 0-255, 0 = instant         */

    int32_t      current_pos;      /* last read position (steps) */
    int32_t      target_pos;
    int32_t      pos_offset;       /* fixed offset added to display (steps) */
    int32_t      pending_delta;    /* async queue for poll task  */
    bool         moving;
    bool         online;
    bool         enabled;
} motor_t;

/* ---- lifecycle ---- */

esp_err_t motor_init(motor_t *m, uint8_t id, motor_type_t type,
                     const char *name, int32_t max_steps, int32_t min_steps,
                     float steps_per_unit, uint16_t speed);

esp_err_t motor_probe(motor_t *m);
esp_err_t motor_enable(motor_t *m);

/* ---- motion (blocking — must call from poll task, NOT LVGL thread) ---- */

esp_err_t motor_move_absolute(motor_t *m, int32_t steps);
esp_err_t motor_move_relative(motor_t *m, int32_t delta);
esp_err_t motor_stop(motor_t *m);
esp_err_t motor_set_zero(motor_t *m);
esp_err_t motor_calibrate(motor_t *m);
esp_err_t motor_read_position(motor_t *m);
esp_err_t motor_homing(motor_t *m);

/* ---- async requests (LVGL-safe — sets pending_delta, returns immediately) ---- */

esp_err_t motor_request_relative(motor_t *m, int32_t delta);
esp_err_t motor_request_absolute(motor_t *m, int32_t steps);

/* ---- background poll task (executes pending_delta + reads positions) ---- */

esp_err_t motor_start_poll_task(motor_t **motors, int count, int interval_ms);

/* ---- unit conversion ---- */

static inline int32_t motor_mm_to_steps(const motor_t *m, float mm)
{ return (int32_t)(mm * m->steps_per_unit); }

static inline int32_t motor_deg_to_steps(const motor_t *m, float deg)
{ return (int32_t)(deg * m->steps_per_unit); }

static inline float motor_steps_to_unit(const motor_t *m, int32_t steps)
{ return (float)steps / m->steps_per_unit; }
