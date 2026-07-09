/*****************************************************************************
 * | File         :   io_extension.c
 ******************************************************************************/
#include "io_extension.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "io_ext";
static bool io_extension_ready = false;

io_extension_obj_t IO_EXTENSION;

bool IO_EXTENSION_Is_Ready(void)
{
    return io_extension_ready;
}

esp_err_t IO_EXTENSION_Init()
{
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow CH32V003 to boot

    // Step 1: Register the I2C device
    esp_err_t ret = DEV_I2C_Set_Slave_Addr(&IO_EXTENSION.addr, IO_EXTENSION_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register I2C device at 0x%02X", IO_EXTENSION_ADDR);
        io_extension_ready = false;
        return ret;
    }

    // Step 2: Probe — actually try to write to verify the device is alive
    uint8_t probe_data[2] = {IO_EXTENSION_Mode, 0xFF};
    ret = DEV_I2C_Write_Nbyte(IO_EXTENSION.addr, probe_data, 2);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "IO extension (CH32V003) at 0x%02X not responding: %s",
                 IO_EXTENSION_ADDR, esp_err_to_name(ret));
        ESP_LOGW(TAG, "→ Backlight & touch-reset will be unavailable");
        io_extension_ready = false;
        return ret;
    }

    io_extension_ready = true;
    IO_EXTENSION.Last_io_value = 0xF7;
    IO_EXTENSION.Last_od_value = 0xF7;

    ESP_LOGI(TAG, "IO extension (CH32V003) ready at 0x%02X", IO_EXTENSION_ADDR);
    return ESP_OK;
}

void IO_EXTENSION_Output(uint8_t pin, uint8_t value)
{
    if (!io_extension_ready) return;

    if (value == 1)
        IO_EXTENSION.Last_io_value |= (1 << pin);
    else
        IO_EXTENSION.Last_io_value &= (~(1 << pin));

    uint8_t data[2] = {IO_EXTENSION_IO_OUTPUT_ADDR, IO_EXTENSION.Last_io_value};
    DEV_I2C_Write_Nbyte(IO_EXTENSION.addr, data, 2);
}

uint8_t IO_EXTENSION_Input(uint8_t pin)
{
    if (!io_extension_ready) return 0;
    uint8_t value = 0;
    DEV_I2C_Read_Nbyte(IO_EXTENSION.addr, IO_EXTENSION_IO_INPUT_ADDR, &value, 1);
    return ((value & (1 << pin)) > 0);
}

void IO_EXTENSION_Pwm_Output(uint8_t Value)
{
    if (!io_extension_ready) return;
    if (Value >= 97) Value = 97;
    uint8_t data[2] = {IO_EXTENSION_PWM_ADDR, Value};
    data[1] = Value * (255 / 100.0);
    DEV_I2C_Write_Nbyte(IO_EXTENSION.addr, data, 2);
}

uint16_t IO_EXTENSION_Adc_Input()
{
    if (!io_extension_ready) return 0;
    uint16_t val = 0;
    DEV_I2C_Read_Word(IO_EXTENSION.addr, IO_EXTENSION_ADC_ADDR, &val);
    return val;
}

uint8_t IO_EXTENSION_RTC_INT_READ()
{
    if (!io_extension_ready) return 0;
    uint16_t val = 0;
    DEV_I2C_Read_Word(IO_EXTENSION.addr, IO_EXTENSION_RTC_INT_ADDR, &val);
    return val;
}
