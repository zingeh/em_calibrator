/*
 * modbus_485.c — RS485 MODBUS RTU transport (manual DE pin)
 *
 * IDF v6.0 removed the old uart_set_mode(UART_MODE_RS485_HALF_DUPLEX)
 * and uart_set_rs485_rts_pin() APIs.  Instead we drive the DE pin
 * manually: HIGH during TX, LOW after TX completes.
 */

#include "modbus_485.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "modbus";

static uart_port_t _port     = UART_NUM_1;
static int        _de_pin    = -1;
static bool       _init_done = false;

/* ---- CRC-16 ---- */

uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* ---- Init ---- */

esp_err_t modbus_485_init(uart_port_t port, int tx_pin, int rx_pin,
                          int de_pin, int baud)
{
    if (_init_done) return ESP_OK;

    _de_pin = de_pin;

    /* DE pin as GPIO output, default LOW (RX mode) */
    gpio_config_t de_conf = {
        .pin_bit_mask = BIT64(de_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&de_conf);
    gpio_set_level(de_pin, 0);

    /* UART — 8N1, no flow control */
    uart_config_t uart_conf = {
        .baud_rate  = baud,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(port, 512, 512, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(port, &uart_conf));
    ESP_ERROR_CHECK(uart_set_pin(port, tx_pin, rx_pin,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    _port = port;
    _init_done = true;
    ESP_LOGI(TAG, "RS485 ready — port=%d TX=%d RX=%d DE=%d baud=%d",
             port, tx_pin, rx_pin, de_pin, baud);
    return ESP_OK;
}

/* ---- Frame helpers ---- */

static size_t build_frame(uint8_t *frame, uint8_t slave, uint8_t func,
                          const uint8_t *payload, size_t payload_len)
{
    frame[0] = slave;
    frame[1] = func;
    memcpy(frame + 2, payload, payload_len);
    uint16_t crc = modbus_crc16(frame, 2 + payload_len);
    frame[2 + payload_len]     = (uint8_t)(crc & 0xFF);
    frame[2 + payload_len + 1] = (uint8_t)(crc >> 8);
    return 2 + payload_len + 2;
}

/* ---- DE helper ---- */
static inline void de_tx(void) { if (_de_pin >= 0) gpio_set_level(_de_pin, 1); }
static inline void de_rx(void) { if (_de_pin >= 0) gpio_set_level(_de_pin, 0); }

/* ---- Transaction ---- */

esp_err_t modbus_485_transaction(uint8_t slave, uint8_t func,
                                  uint16_t reg, uint16_t value,
                                  uint8_t *rx_buf, size_t rx_len,
                                  int timeout_ms)
{
    if (!_init_done) return ESP_ERR_INVALID_STATE;

    /* Payload: reg_hi reg_lo val_hi val_lo */
    uint8_t payload[4];
    payload[0] = (uint8_t)(reg >> 8);
    payload[1] = (uint8_t)(reg & 0xFF);
    payload[2] = (uint8_t)(value >> 8);
    payload[3] = (uint8_t)(value & 0xFF);

    uint8_t frame[8];
    size_t flen = build_frame(frame, slave, func, payload, 4);

    /* Send with manual DE control */
    de_tx();
    uart_write_bytes(_port, frame, flen);
    uart_wait_tx_done(_port, pdMS_TO_TICKS(20));
    de_rx();

    ESP_LOGD(TAG, "TX %02X%02X%02X%02X%02X%02X%02X%02X",
             frame[0], frame[1], frame[2], frame[3],
             frame[4], frame[5], frame[6], frame[7]);

    if (timeout_ms < 0) return ESP_OK;

    /* Receive */
    uint8_t rbuf[256];
    int total = 0, waited = 0;

    /* Flush RX buffer first */
    uart_flush_input(_port);

    while (waited < timeout_ms) {
        int n = uart_read_bytes(_port, rbuf + total,
                                 (int)(sizeof(rbuf) - total - 1),
                                 pdMS_TO_TICKS(10));
        if (n > 0) { total += n; waited = 0; }
        else       { waited += 10; vTaskDelay(pdMS_TO_TICKS(10)); }

        /* MODBUS inter-frame gap is 3.5 chars (~2 ms at 115200).
         * If we've received at least 5 bytes and no more data for
         * 2 polling cycles (20 ms), assume frame is complete. */
        if (total >= 5 && waited >= 20) break;
    }

    if (total < 5) {
        ESP_LOGD(TAG, "RX timeout slave=%d func=0x%02X reg=0x%04X (%d bytes)",
                 slave, func, reg, total);
        return ESP_ERR_TIMEOUT;
    }

    /* CRC check */
    uint16_t crc_rx   = rbuf[total - 2] | ((uint16_t)rbuf[total - 1] << 8);
    uint16_t crc_calc = modbus_crc16(rbuf, total - 2);
    if (crc_rx != crc_calc) {
        ESP_LOGW(TAG, "CRC fail slave=%d rx=0x%04X calc=0x%04X", slave, crc_rx, crc_calc);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Exception check */
    if (rbuf[1] & 0x80) {
        ESP_LOGW(TAG, "Exception slave=%d func=0x%02X code=%d", slave, func, rbuf[2]);
        return ESP_ERR_INVALID_RESPONSE;
    }

    size_t pld_len = total - 2 - 2;
    if (rx_buf && pld_len > 0) {
        size_t copy = (pld_len < rx_len) ? pld_len : rx_len;
        memcpy(rx_buf, rbuf + 2, copy);
    }

    return ESP_OK;
}
