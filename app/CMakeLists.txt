# 主程序：回放二进制 tick 文件并输出聚合统计。
add_executable(marketflow
    main.cpp
)
target_link_libraries(marketflow PRIVATE marketflow_core Threads::Threads)