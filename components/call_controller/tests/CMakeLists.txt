cmake_minimum_required(VERSION 3.16)
project(call_controller_native_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()
add_executable(call_controller_tests
    test_call_controller.c
    ../call_controller.c
    ../../matrix_manager/matrix_manager.c
)
target_include_directories(call_controller_tests PRIVATE
    ../include
    ../../matrix_manager/include
)
add_test(NAME call_controller_tests COMMAND call_controller_tests)
