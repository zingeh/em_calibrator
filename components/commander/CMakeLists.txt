idf_component_register(SRCS "commander.c" "gcode.c"
                        INCLUDE_DIRS "." "../../main"
                        REQUIRES motor driver nvs_flash esp_wifi esp_netif lwip esp_driver_usb_serial_jtag)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-stringop-truncation)
