/*
 * rs485.h — RS485 raw transport (auto-switching transceiver)
 *
 * Used by Emm_V5 binary protocol (NOT MODBUS RTU).
 */

#pragma once

#include "esp_err.h"
#include "driver/uart.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Initialise RS485 UART.
 */
esp_err_t rs485_init(uart_port_t port, int tx_pin, int rx_pin,
                          int de_pin, int baud);

/**
 * @brief Send raw bytes over RS485.
 */
esp_err_t rs485_raw_send(const uint8_t *data, size_t len);

/**
 * @brief Receive raw bytes with timeout.
 * Returns ESP_OK + *rx_len bytes on success, ESP_ERR_TIMEOUT on silence.
 */
esp_err_t rs485_raw_recv(uint8_t *rx, size_t max_len, size_t *rx_len,
                          int timeout_ms);

/**
 * @brief Discard all pending RX bytes.
 */
void rs485_flush_rx(void);
