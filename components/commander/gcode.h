/*
 * gcode.h — G-code parser and dispatcher
 */

#pragma once

#include "motor.h"
#include <stddef.h>

/**
 * @brief Parse a G-code line and dispatch to motor commands.
 *
 * @param line     Null-terminated G-code line (whitespace separated)
 * @param motors   Motor array
 * @param count    Number of motors
 */
void gcode_parse(const char *line, motor_t **motors, int count);
