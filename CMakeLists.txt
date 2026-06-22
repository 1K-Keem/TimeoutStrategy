cmake_minimum_required(VERSION 3.16)

project(TimeoutStrategy LANGUAGES C)

set(CMAKE_C_STANDARD 11)
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

add_executable(timeout_strategy
    src/main.c
    ${CORE_SOURCES}
)
target_include_directories(timeout_strategy PRIVATE include)

enable_testing()
add_executable(run_tests
    tests/test_timeout.c
    ${CORE_SOURCES}
)
target_include_directories(run_tests PRIVATE include)
add_test(NAME run_tests COMMAND run_tests)
