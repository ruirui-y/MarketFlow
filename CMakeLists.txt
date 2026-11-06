cmake_minimum_required(VERSION 3.22)

project(MarketFlow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

find_package(Threads REQUIRED)

# 编译告警按平台分别配置；MSVC 与 GCC 各用各的参数，保证 Windows 上也能直接编译 M1。
if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 主程序：回放二进制 tick 文件并输出聚合结果。
add_executable(marketflow
    src/main.cpp
)
target_include_directories(marketflow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(marketflow PRIVATE Threads::Threads)

# M1 接入聚合核心后，按下面两段展开：核心逻辑进静态库，正确性测试单独成 target。
# add_library(marketflow_core STATIC
#     src/model/tick.cpp
#     src/io/tick_reader.cpp
#     src/aggregate/bar_aggregator.cpp
# )
# target_include_directories(marketflow_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# target_link_libraries(marketflow PRIVATE marketflow_core)

enable_testing()