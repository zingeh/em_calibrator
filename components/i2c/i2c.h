/*****************************************************************************
 * | File         :   i2c.h
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

#ifndef __I2C_H
#define __I2C_H

#include <stdio.h>          // Standard input/output library
#include <string.h>         // String manipulation functions
#include "driver/i2c_master.h"    // ESP32 I2C master driver library
#include "esp_log.h"        // ESP32 logging library for debugging
#include "gpio.h"           // GPIO header for pin configuration

// Define the SDA (data) and SCL (clock) pins for I2C communication
#define EXAMPLE_I2C_MASTER_SDA GPIO_NUM_8  // SDA pin
#define EXAMPLE_I2C_MASTER_SCL GPIO_NUM_9  // SCL pin

// Define the I2C frequency (100 kHz — safer for CH32V003 IO expander)
#define EXAMPLE_I2C_MASTER_FREQUENCY (100 * 1000)  // I2C speed

// Define the I2C master port number (I2C_NUM_0 in this case)
#define EXAMPLE_I2C_MASTER_NUM I2C_NUM_0


typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
} DEV_I2C_Port;
// Function prototypes for I2C communication

/**
 * @brief Initialize the I2C master interface.
 */
DEV_I2C_Port DEV_I2C_Init();

/**
 * @brief Set a new I2C slave address for the device.
 *
 * @param dev_handle Pointer to store the I2C device handle.
 * @param Addr The new I2C address to set for the device.
 * @return ESP_OK on success.
 */
esp_err_t DEV_I2C_Set_Slave_Addr(i2c_master_dev_handle_t *dev_handle, uint8_t Addr);

i2c_master_bus_handle_t DEV_I2C_Get_Bus_Device();

/**
 * @brief Write a single byte to the I2C device.
 * @return ESP_OK on success.
 */
esp_err_t DEV_I2C_Write_Byte(i2c_master_dev_handle_t dev_handle, uint8_t Cmd, uint8_t value);

/**
 * @brief Read a single byte from the I2C device.
 * @param[out] value Pointer to store the read byte.
 * @return ESP_OK on success.
 */
esp_err_t DEV_I2C_Read_Byte(i2c_master_dev_handle_t dev_handle, uint8_t *value);

/**
 * @brief Read a word (2 bytes) from the I2C device.
 * @param[out] value Pointer to store the 16-bit value.
 * @return ESP_OK on success.
 */
esp_err_t DEV_I2C_Read_Word(i2c_master_dev_handle_t dev_handle, uint8_t Cmd, uint16_t *value);

/**
 * @brief Write multiple bytes to the I2C device.
 * @return ESP_OK on success.
 */
esp_err_t DEV_I2C_Write_Nbyte(i2c_master_dev_handle_t dev_handle, uint8_t *pdata, uint8_t len);

/**
 * @brief Read multiple bytes from the I2C device.
 * @return ESP_OK on success.
 */
esp_err_t DEV_I2C_Read_Nbyte(i2c_master_dev_handle_t dev_handle, uint8_t Cmd, uint8_t *pdata, uint8_t len);

#endif
