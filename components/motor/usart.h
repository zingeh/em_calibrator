/*
 * usart.h — bridge: maps STM32 usart_SendCmd → ESP-IDF RS485 raw send
 *
 * Also defines __IO (volatile) needed by Emm_V5.c.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* STM32 HAL compatibility */
#define __IO  volatile

/* Called by every Emm_V5 function to send a command frame */
void usart_SendCmd(volatile uint8_t *cmd, uint16_t len);
