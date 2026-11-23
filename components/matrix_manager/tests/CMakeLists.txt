cmake_minimum_required(VERSION 3.16)
project(matrix_manager_native_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()
add_executable(matrix_manager_tests
    test_matrix_manager.c
    ../matrix_manager.c
)
target_include_directories(matrix_manager_tests PRIVATE ../include)
add_test(NAME matrix_manager_tests COMMAND matrix_manager_tests)
