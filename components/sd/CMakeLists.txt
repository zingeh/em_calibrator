idf_component_register(SRCS "sd.c"  
                        INCLUDE_DIRS "."
                        REQUIRES driver esp_driver_sdmmc fatfs i2c gpio io_extension
                    )
