cmake_minimum_required(VERSION 3.16)

project(TimeoutStrategy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CORE_SOURCES
    src/CSVParser.cpp
    src/DeadlockDetector.cpp
    src/SimulationEngine.cpp
    src/TimeoutManager.cpp
)

add_library(timeout_kim
    src/TimeoutManager.cpp
)
target_include_directories(timeout_kim PRIVATE include)

add_executable(timeout_strategy
    src/main.cpp
    ${CORE_SOURCES}
)
target_include_directories(timeout_strategy PRIVATE include)

enable_testing()
add_executable(run_tests
    tests/test_main.cpp
    ${CORE_SOURCES}
)
target_include_directories(run_tests PRIVATE include)
add_test(NAME run_tests COMMAND run_tests)
