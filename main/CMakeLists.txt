idf_component_register(SRCS "main.c" "menu.c" "display.c" "input.c" "font_data.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_system esp_lcd esp_driver_spi esp_driver_i2c esp_driver_gpio
                                nvs_flash app_update spi_flash esp_app_format)
