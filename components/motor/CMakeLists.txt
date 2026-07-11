idf_component_register(SRCS "motor.c" "Emm_V5.c" "usart.c"
                        INCLUDE_DIRS "."
                        REQUIRES rs485 driver)
