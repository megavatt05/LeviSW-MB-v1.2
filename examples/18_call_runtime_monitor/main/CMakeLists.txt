idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        levisw-mb-v1.2
        lu_event_dispatcher
        call_runtime
        matrix_manager_espidf
        freertos
        esp_timer
        log
)
