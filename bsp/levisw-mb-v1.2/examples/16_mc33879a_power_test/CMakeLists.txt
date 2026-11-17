# Минимальная версия CMake для ESP-IDF >= 5.2
cmake_minimum_required(VERSION 3.16)

# В составе BSP компонент находится на три уровня выше относительно примеров
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../..")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(16_mc33879a_power_test)
