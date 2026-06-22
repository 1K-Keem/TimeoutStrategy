cmake_minimum_required(VERSION 3.16)

project(TimeoutStrategy LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CORE_SOURCES
    src/CSVParser.c
    src/DeadlockDetector.c
    src/SimulationEngine.c
    src/TimeoutManager.c
)

add_library(timeout_kim
    src/TimeoutManager.c
)
target_include_directories(timeout_kim PRIVATE include)
target_compile_definitions(timeout_kim PRIVATE _POSIX_C_SOURCE=200809L)

add_executable(timeout_strategy
    src/main.c
    ${CORE_SOURCES}
)
target_include_directories(timeout_strategy PRIVATE include)
target_compile_definitions(timeout_strategy PRIVATE _POSIX_C_SOURCE=200809L)

enable_testing()
add_executable(run_tests
    tests/test_timeout.c
    ${CORE_SOURCES}
)
target_include_directories(run_tests PRIVATE include)
target_compile_definitions(run_tests PRIVATE _POSIX_C_SOURCE=200809L)
add_test(NAME run_tests COMMAND run_tests)
