# 行情模型、二进制读取、窗口聚合。头文件就放在对应 cpp 旁边，不单独拆 include 目录。
add_library(marketflow_core STATIC
    io/tick_reader.cpp
    aggregate/bar_aggregator.cpp
)

# include 根设成仓库根，所以代码里写 marketflow/io/tick_reader.h 就能找到。
target_include_directories(marketflow_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(marketflow_core PUBLIC marketflow_build_options)