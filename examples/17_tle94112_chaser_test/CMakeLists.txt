# Минимальная версия CMake для ESP-IDF >= 5.2
cmake_minimum_required(VERSION 3.16)

# Путь к компоненту пакета поддержки платы (BSP) LeviSW-MB v1.2
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp/levisw-mb-v1.2"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/lu_decoder"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/lu_event_dispatcher"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(17_tle94112_chaser_test)
