/*****************************************************************************
 * | File         :   i2c.c
 * | Author       :   Waveshare team
 * | Function     :   Hardware underlying interface
 * | Info         :
 * |                 I2C driver code for I2C communication.
 * ----------------
 * | This version :   V1.0
 * | Date         :   2024-11-26
 * | Info         :   Basic version
 *
 ******************************************************************************/

#include "i2c.h"  // Include I2C driver header for I2C functions
#include "esp_check.h"

static const char *TAG = "i2c";  // Define a tag for logging

// Global handle for the I2C master bus
DEV_I2C_Port handle;

DEV_I2C_Port DEV_I2C_Init()
{
    // Define I2C bus configuration parameters
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = EXAMPLE_I2C_MASTER_NUM,
        .scl_io_num = EXAMPLE_I2C_MASTER_SCL,
        .sda_io_num = EXAMPLE_I2C_MASTER_SDA,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &handle.bus));

    ESP_LOGI(TAG, "I2C bus initialized on SDA=GPIO%d, SCL=GPIO%d",
             EXAMPLE_I2C_MASTER_SDA, EXAMPLE_I2C_MASTER_SCL);

    return handle;
}

esp_err_t DEV_I2C_Set_Slave_Addr(i2c_master_dev_handle_t *dev_handle, uint8_t Addr)
{
    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = EXAMPLE_I2C_MASTER_FREQUENCY,
        .device_address = Addr,
    };

    esp_err_t ret = i2c_master_bus_add_device(handle.bus, &i2c_dev_conf, dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add I2C device at address 0x%02X: %s", Addr, esp_err_to_name(ret));
    }
    return ret;
}

i2c_master_bus_handle_t DEV_I2C_Get_Bus_Device()
{
    return handle.bus;
}

esp_err_t DEV_I2C_Write_Byte(i2c_master_dev_handle_t dev_handle, uint8_t Cmd, uint8_t value)
{
    uint8_t data[2] = {Cmd, value};
    esp_err_t ret = i2c_master_transmit(dev_handle, data, sizeof(data), 100);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C write byte failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t DEV_I2C_Read_Byte(i2c_master_dev_handle_t dev_handle, uint8_t *value)
{
    uint8_t data[1] = {0};
    esp_err_t ret = i2c_master_receive(dev_handle, data, 1, 100);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C read byte failed: %s", esp_err_to_name(ret));
        return ret;
    }
    *value = data[0];
    return ESP_OK;
}

esp_err_t DEV_I2C_Read_Word(i2c_master_dev_handle_t dev_handle, uint8_t Cmd, uint16_t *value)
{
    uint8_t data[2] = {Cmd};
    esp_err_t ret = i2c_master_transmit_receive(dev_handle, data, 1, data, 2, 100);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C read word failed: %s", esp_err_to_name(ret));
        return ret;
    }
    *value = data[1] << 8 | data[0];
    return ESP_OK;
}

esp_err_t DEV_I2C_Write_Nbyte(i2c_master_dev_handle_t dev_handle, uint8_t *pdata, uint8_t len)
{
    esp_err_t ret = i2c_master_transmit(dev_handle, pdata, len, 100);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C write %d bytes failed: %s", len, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t DEV_I2C_Read_Nbyte(i2c_master_dev_handle_t dev_handle, uint8_t Cmd, uint8_t *pdata, uint8_t len)
{
    esp_err_t ret = i2c_master_transmit_receive(dev_handle, &Cmd, 1, pdata, len, 100);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C read %d bytes failed: %s", len, esp_err_to_name(ret));
    }
    return ret;
}
