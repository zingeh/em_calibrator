/*
 * ui.h — EM Calibrator main control screen
 */

#pragma once

#include "lvgl.h"
#include "motor.h"

/**
 * @brief Build the EM Calibrator control screen.
 *
 * @param motors  Array of 5 motor_t pointers:
 *                [0] distance, [1] base yaw, [2] base pitch,
 *                [3] tracker yaw, [4] tracker pitch.
 *
 * Call once after lvgl_port_init().  The UI runs its own LVGL
 * timer to refresh position readouts every 200 ms.
 */
void ui_init(motor_t *motors[5]);
