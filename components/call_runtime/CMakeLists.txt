idf_component_register(
    SRCS "call_runtime.c"
    INCLUDE_DIRS "include"
    REQUIRES lu_decoder handset_detector call_controller matrix_manager
)
