/*
 * app_config.h — EM Calibrator application configuration
 *
 * Firmware version, motor IDs, RS485 pin assignments, and safety limits.
 */

#pragma once

#include "driver/gpio.h"
#include "driver/uart.h"

/* ---- Firmware identity ---- */
#define APP_NAME       "EM_CALIBRATOR"
#define APP_VERSION    "v1.0.0"

/* ---- RS485 bus ---- */
#define RS485_UART_PORT    UART_NUM_0        /* Board RS485 is wired to UART0     */
#define RS485_TX_PIN       GPIO_NUM_44       /* GPIO44 = UART0_TXD (ROM default)  */
#define RS485_RX_PIN       GPIO_NUM_43       /* GPIO43 = UART0_RXD (ROM default)  */
#define RS485_DE_PIN       (-1)              /* auto-switching RS485, no DE        */
#define RS485_BAUD         115200

/* ---- Motor MODBUS IDs ---- */
#define MOTOR_ID_DISTANCE     1   /* Lead screw — base/tracker distance   */
#define MOTOR_ID_BASE_YAW     2   /* Base yaw axis                        */
#define MOTOR_ID_BASE_PITCH   3   /* Base pitch axis                      */
#define MOTOR_ID_TRACK_YAW    4   /* Tracker yaw axis                     */
#define MOTOR_ID_TRACK_PITCH  5   /* Tracker pitch axis                   */

/* ---- Motor speed defaults (MODBUS register 0x0385, RPM units) ---- */
#define MOTOR_SPEED_DISTANCE    200
#define MOTOR_SPEED_ANGLE       120

/* ---- Stepper resolution (steps per revolution) ---- */
#define MOTOR_STEPS_PER_REV     3200

/* ---- Lead screw pitch: mm per full revolution ---- */
#define LEAD_SCREW_PITCH_MM     10.0f

/* ---- Safety limits ---- */
#define DISTANCE_MAX_MM         1200.0f
#define DISTANCE_MIN_MM         0.0f
#define YAW_MAX_DEG              180.0f
#define YAW_MIN_DEG             -180.0f
#define PITCH_MAX_DEG             0.0f
#define PITCH_MIN_DEG           -90.0f

/* ---- UI refresh interval (ms) ---- */
#define UI_REFRESH_MS          200
