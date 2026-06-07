cmake_minimum_required(VERSION 3.16)

project(TimeoutStrategy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(timeout_kim
    src/TimeoutManager.cpp
)

target_include_directories(timeout_kim PRIVATE include)

# Quang/Khanh/Khoa se mo lai executable sau khi hoan thanh cac phan con lai:
# add_executable(timeout_strategy
#     src/main.cpp
#     src/CSVParser.cpp
#     src/DeadlockDetector.cpp
#     src/SimulationEngine.cpp
#     src/TimeoutManager.cpp
# )
# target_include_directories(timeout_strategy PRIVATE include)
