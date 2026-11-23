cmake_minimum_required(VERSION 3.16)
project(handset_detector_native_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()
add_executable(handset_detector_tests
    test_handset_detector.c
    ../handset_detector.c
)
target_include_directories(handset_detector_tests PRIVATE ../include)
add_test(NAME handset_detector_tests COMMAND handset_detector_tests)
