idf_component_register(SRCS "modbus_485.c"
                        INCLUDE_DIRS "."
                        REQUIRES driver esp_driver_uart esp_driver_gpio)
