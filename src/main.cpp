#include <cstdio>

// MarketFlow 主程序入口：当前只打印版本与用法，M1 的数据通路尚未接入。
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::printf("MarketFlow 0.1.0\n");
    std::printf("用法: marketflow <ticks.bin>\n");
    std::printf("状态: M1 脚手架，聚合逻辑未接入\n");

    return 0;
}