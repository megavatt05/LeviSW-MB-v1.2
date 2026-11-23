cmake_minimum_required(VERSION 3.16)
project(lu_decoder_native_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()
add_executable(lu_decoder_tests
    test_lu_decoder.c
    ../lu_decoder.c
)
target_include_directories(lu_decoder_tests PRIVATE ../include)
add_test(NAME lu_decoder_tests COMMAND lu_decoder_tests)
