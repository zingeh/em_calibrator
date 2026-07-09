/*****************************************************************************
 * | File       :   main.c
 * | Author     :   Waveshare team
 * | Function   :   Main function
 * | Info       :   Ported LVGL 9.2.0 and display the official demo interface
 * | Version    :   V1.0
 * | Date       :   2025-07-30
 * | Language   :   C (ESP-IDF)
 ******************************************************************************/

#include "rgb_lcd_port.h"
#include "gt911.h"
#include "esp_check.h"
#include "lv_demos.h"
#include "lvgl_port.h"

static const char *TAG = "main";

/* ---- I2C bus scanner ---- */
static void i2c_scan(i2c_master_bus_handle_t bus)
{
    ESP_LOGI(TAG, "=== I2C Bus Scan ===");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        esp_err_t ret = i2c_master_probe(bus, addr, 50);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Found device at 0x%02X (%d)", addr, addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "=== Scan complete: %d device(s) found ===", found);
}

void app_main()
{
    static esp_lcd_panel_handle_t panel_handle = NULL;
    static esp_lcd_touch_handle_t tp_handle = NULL;

    // Init I2C bus
    ESP_LOGI(TAG, "Initializing I2C bus...");
    DEV_I2C_Init();

    // Scan I2C bus
    i2c_scan(DEV_I2C_Get_Bus_Device());

    // Init IO extension (CH32V003) — non-fatal
    ESP_LOGI(TAG, "Initializing IO extension...");
    esp_err_t io_ret = IO_EXTENSION_Init();
    if (io_ret != ESP_OK) {
        ESP_LOGW(TAG, "IO extension unavailable (err 0x%x)", io_ret);
    }

    // Try touch init — non-fatal
    ESP_LOGI(TAG, "Initializing touch controller...");
    tp_handle = touch_gt911_init(DEV_I2C_Get_Bus_Device());
    if (tp_handle == NULL) {
        ESP_LOGW(TAG, "Touch controller unavailable — touch input disabled");
    }

    // Init LCD panel — must succeed
    ESP_LOGI(TAG, "Initializing RGB LCD...");
    panel_handle = waveshare_esp32_s3_rgb_lcd_init();

    // Init LVGL
    ESP_LOGI(TAG, "Initializing LVGL...");
    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle));

    ESP_LOGI(TAG, "Display LVGL widget demo");
    if (lvgl_port_lock(-1)) {
        lv_demo_widgets();
        lvgl_port_unlock();
    }

    // Turn on backlight
    waveshare_rgb_lcd_bl_on();

    ESP_LOGI(TAG, "Init complete");
}
