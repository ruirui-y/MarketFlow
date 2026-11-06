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

# 全部产物静态链接：MSVC 走 /MT，GCC 走 -static，目标机器不需要另装运行库。
# Linux 上链接报 cannot find -lc，说明缺静态 libc，装 libc6-dev 即可。
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# 所有目标共用的编译选项，子目录里的 target_link_libraries 挂上它就行。
add_library(marketflow_build_options INTERFACE)

if(MSVC)
    target_compile_options(marketflow_build_options INTERFACE /W4 /utf-8 /permissive-)
    target_compile_definitions(marketflow_build_options INTERFACE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(marketflow_build_options INTERFACE -Wall -Wextra -Wpedantic)
    target_link_options(marketflow_build_options INTERFACE -static)
endif()

add_subdirectory(marketflow)
add_subdirectory(app)

enable_testing()
add_subdirectory(tests)