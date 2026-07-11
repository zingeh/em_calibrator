/*
 * usart.c — ESP-IDF bridge for STM32 usart_SendCmd
 */

#include "usart.h"
#include "rs485.h"
#include "esp_log.h"

static const char *TAG = "emm_tx";

void usart_SendCmd(volatile uint8_t *cmd, uint16_t len)
{
    char d[80] = {0}; int o = 0;
    for (int i = 0; i < len && o < 75; i++)
        o += snprintf(d + o, sizeof(d) - o, "%02X ", cmd[i]);
    ESP_LOGI(TAG, "TX %d B: %s", len, d);

    rs485_raw_send((const uint8_t *)cmd, len);
}
