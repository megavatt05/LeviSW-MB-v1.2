idf_component_register(
    SRCS "matrix_manager_espidf.c"
    INCLUDE_DIRS "include"
    REQUIRES matrix_manager levisw-mb-v1.2 tle94112 mc33879a esp_timer driver
        esp_rom
)
