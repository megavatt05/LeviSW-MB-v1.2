cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp/levisw-mb-v1.2"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/matrix_manager"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/call_controller"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/call_runtime"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/handset_detector"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/lu_decoder"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/lu_event_dispatcher"
    "${CMAKE_CURRENT_LIST_DIR}/components/tle94112"
    "${CMAKE_CURRENT_LIST_DIR}/components/matrix_manager_adg_tle"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(20_switchboard_adg_tle_no_mc33879)
