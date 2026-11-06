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

# 全部产物静态链接：MSVC 走 /MT，GCC 走 -static，目标机器不需要装运行库。
# 如果 Linux 上链接报 cannot find -lc，说明缺静态 libc，装 libc6-dev 即可。
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_link_options(-static)
endif()

# 编译告警按平台分别配置；MSVC 与 GCC 各用各的参数。
if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 聚合核心：行情模型、二进制读取、窗口聚合。主程序与测试都链接它。
add_library(marketflow_core STATIC
    src/io/tick_reader.cpp
    src/aggregate/bar_aggregator.cpp
)
target_include_directories(marketflow_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 主程序：回放二进制 tick 文件并输出聚合统计。
add_executable(marketflow
    src/main.cpp
)
target_link_libraries(marketflow PRIVATE marketflow_core Threads::Threads)

# 正确性测试：流式聚合结果必须与暴力重算逐条一致。
add_executable(marketflow_aggregate_test
    tests/bar_aggregator_test.cpp
)
target_link_libraries(marketflow_aggregate_test PRIVATE marketflow_core)

enable_testing()
add_test(NAME marketflow_aggregate_test COMMAND marketflow_aggregate_test)
set_tests_properties(marketflow_aggregate_test PROPERTIES RUN_SERIAL TRUE)