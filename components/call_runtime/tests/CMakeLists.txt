cmake_minimum_required(VERSION 3.16)
project(call_runtime_native_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()
add_executable(call_runtime_tests
    test_call_runtime.c
    ../call_runtime.c
    ../../handset_detector/handset_detector.c
    ../../lu_decoder/lu_decoder.c
    ../../call_controller/call_controller.c
    ../../matrix_manager/matrix_manager.c
)
target_include_directories(call_runtime_tests PRIVATE
    ../include
    ../../handset_detector/include
    ../../lu_decoder/include
    ../../call_controller/include
    ../../matrix_manager/include
)
add_test(NAME call_runtime_tests COMMAND call_runtime_tests)
