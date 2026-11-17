# Минимальная версия CMake для ESP-IDF >= 5.0
cmake_minimum_required(VERSION 3.16)

# Путь к компоненту пакета поддержки платы (BSP) LeviSW-MB v1.2
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../bsp/levisw-mb-v1.2")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(06_mcu_esp32_rf)
