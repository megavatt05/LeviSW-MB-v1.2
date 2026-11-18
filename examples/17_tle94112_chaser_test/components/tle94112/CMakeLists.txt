idf_component_register(
    SRCS "tle94112.c" "example/tle94112_example.c"
    INCLUDE_DIRS "include" "example"
    REQUIRES driver esp_driver_spi esp_driver_gpio
)
