/*****************************************************************************
 * | File         :   io_extension.h
 * | Author       :   Waveshare team
 * | Function     :   GPIO control using io extension via I2C interface
 * | Info         :
 * |                 Header file for controlling GPIO pins via the io extension
 * |                 chip using I2C communication.
 * ----------------
 * | This version :   V1.0
 * | Date         :   2024-11-19
 * | Info         :   Basic version
 *
 ******************************************************************************/

 #ifndef __IO_EXTENSION_H
 #define __IO_EXTENSION_H

 #include "esp_err.h"
 #include "i2c.h"

 /* IO EXTENSION Function Register Addresses */
 #define IO_EXTENSION_ADDR          0x24

 /* Mode control flags */
 #define IO_EXTENSION_Mode             0x02
 #define IO_EXTENSION_IO_OUTPUT_ADDR   0x03
 #define IO_EXTENSION_IO_INPUT_ADDR    0x04
 #define IO_EXTENSION_PWM_ADDR         0x05
 #define IO_EXTENSION_ADC_ADDR         0x06
 #define IO_EXTENSION_RTC_INT_ADDR     0x07

 /* Specific IO pin assignments */
 #define IO_EXTENSION_IO_0          0x00
 #define IO_EXTENSION_IO_1          0x01  // Touch reset
 #define IO_EXTENSION_IO_2          0x02  // Backlight control
 #define IO_EXTENSION_IO_3          0x03  // PA
 #define IO_EXTENSION_IO_4          0x04  // SD card CS pin
 #define IO_EXTENSION_IO_5          0x05
 #define IO_EXTENSION_IO_6          0x06
 #define IO_EXTENSION_IO_7          0x07

 /* Structure to represent the IO EXTENSION device */
 typedef struct _io_extension_obj_t {
     i2c_master_dev_handle_t addr;
     uint8_t Last_io_value;
     uint8_t Last_od_value;
 } io_extension_obj_t;

 /**
  * @brief Initialize the IO_EXTENSION device.
  * @return ESP_OK on success, error code if CH32V003 is not responding.
  */
 esp_err_t IO_EXTENSION_Init();

 /**
  * @brief Check if IO extension is available.
  * @return true if the IO extension was successfully initialized.
  */
 bool IO_EXTENSION_Is_Ready(void);

 /**
  * @brief Set IO pin output (high/low). No-op if IO extension is not ready.
  */
 void IO_EXTENSION_Output(uint8_t pin, uint8_t value);

 uint8_t IO_EXTENSION_Input(uint8_t pin);
 void IO_EXTENSION_Pwm_Output(uint8_t Value);
 uint16_t IO_EXTENSION_Adc_Input();
 uint8_t IO_EXTENSION_RTC_INT_READ();

 #endif  // __IO_EXTENSION_H
