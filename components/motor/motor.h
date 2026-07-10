/*
 * motor.h — Stepper motor abstraction over ZDT MODBUS RTU
 *
 * Each motor is a MODBUS slave with its own ID on the shared RS485 bus.
 * Provides position-mode movement with software limit clamping.
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/* ---- Motor type ---- */
typedef enum {
    MOTOR_LINEAR = 0,   /* Lead screw / linear stage  */
    MOTOR_ROTARY = 1,   /* Rotary axis (yaw / pitch)  */
} motor_type_t;

/* ---- ZDT MODBUS registers ---- */
#define REG_TARGET_POS_LO    0x0386   /* Target position, low 16 bits  */
#define REG_TARGET_POS_HI    0x0387   /* Target position, high 16 bits */
#define REG_SPEED            0x0385   /* Running speed (RPM)           */
#define REG_POS_ARRIVED      0x038B   /* Position-arrival flag (read)  */
#define REG_CURRENT_POS      0x0030   /* Real-time pulse count (6 B)   */
#define REG_ALARM             0x0064   /* Alarm code                    */
#define REG_CALIBRATE         0x0006   /* Encoder calibration           */
#define REG_CLEAR_POS         0x000A   /* Position clear (set zero)     */

/* ---- Motor state ---- */
typedef struct motor {
    uint8_t      id;             /* MODBUS slave address (1-247)       */
    motor_type_t type;           /* Linear or rotary                   */
    char         name[16];       /* Human-readable label               */

    /* Limits (in steps) */
    int32_t      max_steps;      /* Firmware clamp maximum             */
    int32_t      min_steps;      /* Firmware clamp minimum             */

    /* Conversion */
    float        steps_per_unit; /* Steps per mm (linear) or deg (rot) */

    /* Speed (RPM, MODBUS register 0x0385) */
    uint16_t     speed;

    /* Cached state */
    int32_t      current_pos;    /* Last-known position (steps)        */
    int32_t      target_pos;     /* Commanded position (steps)         */
    bool         moving;         /* Movement in progress               */
    bool         online;         /* MODBUS communication OK            */
} motor_t;

/* ---- Public API ---- */

/**
 * @brief Initialise a motor's metadata.  Does NOT talk to the bus.
 *
 * Call once per motor at startup.  Call motor_probe() separately to
 * test whether the motor actually answers on the bus.
 */
esp_err_t motor_init(motor_t *m, uint8_t id, motor_type_t type,
                     const char *name, int32_t max_steps, int32_t min_steps,
                     float steps_per_unit, uint16_t speed);

/**
 * @brief Probe the motor over MODBUS to check it is online.
 */
esp_err_t motor_probe(motor_t *m);

/**
 * @brief Move to absolute position (steps), clamped to [min, max].
 */
esp_err_t motor_move_absolute(motor_t *m, int32_t steps);

/**
 * @brief Move by relative offset (steps), clamped to [min, max].
 */
esp_err_t motor_move_relative(motor_t *m, int32_t delta);

/**
 * @brief Stop the motor immediately.
 */
esp_err_t motor_stop(motor_t *m);

/**
 * @brief Set current position as zero (MODBUS register 0x000A).
 */
esp_err_t motor_set_zero(motor_t *m);

/**
 * @brief Encoder calibration (MODBUS register 0x0006).
 */
esp_err_t motor_calibrate(motor_t *m);

/**
 * @brief Read real-time position from motor into motor->current_pos.
 *
 * Also updates motor->online and motor->moving flags.
 */
esp_err_t motor_read_position(motor_t *m);

/**
 * @brief Homing stub — insert application-specific limit-switch logic.
 *
 * Currently reduces to motor_set_zero() and logs a reminder.
 */
esp_err_t motor_homing(motor_t *m);

/**
 * @brief Start a background FreeRTOS task that polls motor positions.
 *
 * @param motors   Pointer array of motor_t pointers.
 * @param count    Number of entries in the array.
 * @param interval_ms  Polling interval in milliseconds.
 *
 * Call once after all motors have been initialised.
 * The task runs at priority 1 with a 3 KB stack.
 */
esp_err_t motor_start_poll_task(motor_t **motors, int count, int interval_ms);

/* ---- Helpers ---- */

/** Convert mm to steps for a linear motor. */
static inline int32_t motor_mm_to_steps(const motor_t *m, float mm)
{
    return (int32_t)(mm * m->steps_per_unit);
}

/** Convert degrees to steps for a rotary motor. */
static inline int32_t motor_deg_to_steps(const motor_t *m, float deg)
{
    return (int32_t)(deg * m->steps_per_unit);
}

/** Convert current_pos steps to human-readable unit. */
static inline float motor_steps_to_unit(const motor_t *m, int32_t steps)
{
    return (float)steps / m->steps_per_unit;
}
