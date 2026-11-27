idf_component_register(
    SRCS "matrix_manager_adg_tle.c"
    INCLUDE_DIRS "include"
    REQUIRES matrix_manager levisw-mb-v1.2 tle94112 esp_rom
)
