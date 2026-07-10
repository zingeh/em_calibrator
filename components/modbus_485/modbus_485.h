/*
 * modbus_485.h — RS485 MODBUS RTU transport layer
 *
 * Half-duplex RS485 via ESP32-S3 UART with manual DE pin control.
 * Handles frame construction, CRC16, send/receive with timeout.
 */

#pragma once

#include "esp_err.h"
#include "driver/uart.h"
#include <stdint.h>
#include <stddef.h>

/* ---- MODBUS function codes ---- */
#define MODBUS_FC_READ_HOLDING    0x03
#define MODBUS_FC_READ_INPUT      0x04
#define MODBUS_FC_WRITE_SINGLE    0x06
#define MODBUS_FC_WRITE_MULTI     0x10

/**
 * @brief Initialise RS485 UART.
 *
 * @param port       UART port (UART_NUM_1 recommended)
 * @param tx_pin     TX / RS485-DI pin
 * @param rx_pin     RX / RS485-RO pin
 * @param de_pin     DE+RE driver-enable pin (connected to RTS)
 * @param baud       Baud rate (default 115200)
 * @return ESP_OK on success
 */
esp_err_t modbus_485_init(uart_port_t port, int tx_pin, int rx_pin,
                          int de_pin, int baud);

/**
 * @brief Send a MODBUS frame and (optionally) read the reply.
 *
 * Common patterns:
 *   - Write single register (FC 0x06):
 *       modbus_485_transaction(id, MODBUS_FC_WRITE_SINGLE, reg, value,
 *                              NULL, 0, -1)
 *   - Read registers (FC 0x03 / 0x04):
 *       modbus_485_transaction(id, MODBUS_FC_READ_INPUT, reg, len,
 *                              rx_buf, rx_len, 50)
 *
 * @param slave      Slave ID (1-247)
 * @param func       MODBUS function code
 * @param reg        Starting register address (16-bit)
 * @param value      Register value / count (16-bit; meaning depends on func)
 * @param rx_buf     Buffer for response payload (excl. slave+func+len+crc);
 *                   may be NULL if no response expected.
 * @param rx_len     Expected payload length in bytes
 * @param timeout_ms Receive timeout (ms). -1 = no receive (send only).
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no reply,
 *         ESP_ERR_INVALID_RESPONSE on CRC / length mismatch.
 */
esp_err_t modbus_485_transaction(uint8_t slave, uint8_t func,
                                  uint16_t reg, uint16_t value,
                                  uint8_t *rx_buf, size_t rx_len,
                                  int timeout_ms);

/**
 * @brief Compute MODBUS CRC-16.
 *
 * @param data  Buffer
 * @param len   Number of bytes
 * @return CRC-16 (little-endian: low byte at &lt;data+len&gt;,
 *         high byte at &lt;data+len+1&gt;).
 */
uint16_t modbus_crc16(const uint8_t *data, size_t len);
