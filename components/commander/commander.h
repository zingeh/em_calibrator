/*
 * commander.h — G-code remote control (USB CDC + WiFi TCP)
 */

#pragma once

#include "esp_err.h"
#include "motor.h"

typedef struct {
    motor_t **motors;
    int      motor_count;
} commander_cfg_t;

/**
 * @brief Initialise G-code commander task.
 *
 * Starts USB CDC device and WiFi TCP server (port 8888) in a
 * background FreeRTOS task.  G-code commands received on either
 * transport are dispatched to the motor async API.
 */
esp_err_t commander_init(const commander_cfg_t *cfg);

/**
 * @brief Write a response line to the active client transport.
 *
 * Called by gcode_parse callbacks.
 */
void commander_reply(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Get WiFi status string for UI display.
 *
 * Returns e.g. "WiFi: 192.168.1.100" when connected,
 * "WiFi: connecting..." while in progress,
 * or "WiFi: off" when not configured.
 */
const char *commander_get_wifi_status(void);
