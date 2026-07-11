/*
 * rs485.c — RS485 raw transport (auto-switching)
 *
 * Used by Emm_V5 binary protocol over half-duplex RS485.
 * Console moved to USB JTAG — UART0 dedicated to RS485.
 */

#include "rs485.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "rs485";

static uart_port_t _port     = UART_NUM_0;
static int        _de_pin    = -1;
static bool       _init_done = false;

/* ---- Init ---- */

esp_err_t rs485_init(uart_port_t port, int tx_pin, int rx_pin,
                          int de_pin, int baud)
{
    if (_init_done) return ESP_OK;

    _de_pin = de_pin;

    if (de_pin >= 0) {
        gpio_config_t c = { .pin_bit_mask = BIT64(de_pin),
                            .mode = GPIO_MODE_OUTPUT,
                            .pull_up_en = GPIO_PULLUP_DISABLE,
                            .pull_down_en = GPIO_PULLDOWN_DISABLE };
        gpio_config(&c);
        gpio_set_level(de_pin, 0);
    }

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

    _port = port; _init_done = true;

    uart_flush_input(port);

    ESP_LOGI(TAG, "ready — port=%d TX=%d RX=%d DE=%d baud=%d",
             port, tx_pin, rx_pin, de_pin, baud);
    return ESP_OK;
}

/* ---- DE helpers ---- */

static inline void de_tx(void) { if (_de_pin >= 0) gpio_set_level(_de_pin, 1); }
static inline void de_rx(void) { if (_de_pin >= 0) gpio_set_level(_de_pin, 0); }

/* ---- Raw send / receive ---- */

esp_err_t rs485_raw_send(const uint8_t *data, size_t len)
{
    if (!_init_done) return ESP_ERR_INVALID_STATE;
    de_tx();
    uart_write_bytes(_port, data, len);
    uart_wait_tx_done(_port, pdMS_TO_TICKS(20));
    de_rx();
    return ESP_OK;
}

esp_err_t rs485_raw_recv(uint8_t *rx, size_t max_len, size_t *rx_len,
                          int timeout_ms)
{
    if (!_init_done) return ESP_ERR_INVALID_STATE;
    *rx_len = 0;
    int waited = 0;
    while (waited < timeout_ms) {
        int n = uart_read_bytes(_port, rx + *rx_len,
                                 (int)(max_len - *rx_len - 1),
                                 pdMS_TO_TICKS(10));
        if (n > 0) { *rx_len += n; waited = 0; if (*rx_len >= max_len) break; }
        else       { waited += 10; vTaskDelay(pdMS_TO_TICKS(10)); }
        if (*rx_len > 0 && waited >= 15) break;
    }
    return (*rx_len > 0) ? ESP_OK : ESP_ERR_TIMEOUT;
}

void rs485_flush_rx(void)
{
    uart_flush_input(_port);
}
