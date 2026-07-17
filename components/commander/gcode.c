/*
 * gcode.c — G-code dispatcher
 *
 * Commands:
 *   G0 D<mm> [R]  — distance    (R = relative, otherwise absolute)
 *   G1 B<deg>     — base yaw    (absolute)
 *   G1 P<deg>     — base pitch  (absolute)
 *   G2 T<deg>     — tracker yaw (absolute)
 *   G2 Q<deg>     — tracker pitch (absolute)
 *   G28           — homing all
 *   M115          — firmware info
 *   M114          — report positions
 */

#include "gcode.h"
#include "commander.h"
#include "app_config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ---- G-code dispatch ---- */

static void cmd_g0(motor_t **m, int n, float d, bool relative)
{
    if (n < 1 || !m[0] || !m[0]->online) {
        commander_reply("error: distance motor offline\r\n");
        return;
    }
    int32_t steps;
    if (relative) {
        steps = motor_mm_to_steps(m[0], d);
        motor_request_relative(m[0], steps);
        commander_reply("ok rel D %.1f mm\r\n", d);
    } else {
        steps = motor_mm_to_steps(m[0], d);
        motor_request_absolute(m[0], steps);
        commander_reply("ok abs D %.1f mm\r\n", d);
    }
}

static void cmd_g1(motor_t **m, int n, char axis, float deg)
{
    int idx = -1;
    const char *name = "?";
    if (axis == 'B') { idx = 1; name = "base yaw"; }
    if (axis == 'P') { idx = 2; name = "base pitch"; }
    if (idx < 1 || idx >= n || !m[idx] || !m[idx]->online) {
        commander_reply("error: motor %s offline\r\n", name);
        return;
    }
    int32_t steps = motor_deg_to_steps(m[idx], deg);
    motor_request_absolute(m[idx], steps);
    commander_reply("ok %c %.1f deg\r\n", axis, deg);
}

static void cmd_g2(motor_t **m, int n, char axis, float deg)
{
    int idx = -1;
    const char *name = "?";
    if (axis == 'T') { idx = 3; name = "tracker yaw"; }
    if (axis == 'Q') { idx = 4; name = "tracker pitch"; }
    if (idx < 3 || idx >= n || !m[idx] || !m[idx]->online) {
        commander_reply("error: motor %s offline\r\n", name);
        return;
    }
    int32_t steps = motor_deg_to_steps(m[idx], deg);
    motor_request_absolute(m[idx], steps);
    commander_reply("ok %c %.1f deg\r\n", axis, deg);
}

static void cmd_g28(motor_t **m, int n)
{
    int cnt = 0;
    for (int i = 0; i < n; i++)
        if (m[i] && m[i]->online) { motor_request_relative(m[i], -m[i]->target_pos); cnt++; }
    commander_reply("ok homing %d motors\r\n", cnt);
}

static void cmd_m115(void)
{
    commander_reply("ok EM_CALIBRATOR %s\r\n", APP_VERSION);
}

static void cmd_m114(motor_t **m, int n)
{
    char buf[128];
    int off = 0;
    for (int i = 0; i < n && i < 5; i++) {
        if (!m[i]) continue;
        float val = motor_steps_to_unit(m[i], m[i]->current_pos);
        if (m[i]->type == MOTOR_LINEAR)
            off += snprintf(buf + off, sizeof(buf) - off,
                            " D%.1f", val / 10.0f);
        else
            off += snprintf(buf + off, sizeof(buf) - off,
                            " %s%.1f", (i==1?"B":i==2?"P":i==3?"T":"Q"), val);
    }
    commander_reply("ok%s\r\n", buf);
}

/* ---- Parser ---- */

void gcode_parse(const char *line, motor_t **motors, int count)
{
    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;
    if (!*line) return;

    if (*line == 'G') {
        int code = atoi(line + 1);
        const char *p = line;
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;

        switch (code) {
        case 0: {
            float d = 0; bool rel = false;
            while (*p) {
                if (*p == 'D') { d = (float)atof(p + 1); while (*p && *p != ' ') p++; }
                else if (*p == 'R') { rel = true; p++; }
                else p++;
                while (*p == ' ') p++;
            }
            cmd_g0(motors, count, d, rel);
            break;
        }
        case 1: {
            float deg = 0; char axis = 0;
            while (*p) {
                if (*p == 'B' || *p == 'P') { axis = *p; deg = (float)atof(p + 1); while (*p && *p != ' ') p++; }
                else p++;
                while (*p == ' ') p++;
            }
            if (axis) cmd_g1(motors, count, axis, deg);
            break;
        }
        case 2: {
            float deg = 0; char axis = 0;
            while (*p) {
                if (*p == 'T' || *p == 'Q') { axis = *p; deg = (float)atof(p + 1); while (*p && *p != ' ') p++; }
                else p++;
                while (*p == ' ') p++;
            }
            if (axis) cmd_g2(motors, count, axis, deg);
            break;
        }
        case 28:
            cmd_g28(motors, count);
            break;
        default:
            commander_reply("error: unknown G-code G%d\r\n", code);
            break;
        }
    } else if (*line == 'M') {
        int code = atoi(line + 1);
        switch (code) {
        case 115: cmd_m115(); break;
        case 114: cmd_m114(motors, count); break;
        default: commander_reply("error: unknown M-code M%d\r\n", code); break;
        }
    } else {
        commander_reply("error: unknown command\r\n");
    }
}
