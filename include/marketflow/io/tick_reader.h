#pragma once

#include "marketflow/model/tick.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace marketflow
{

// 从 ticks.bin 里按块读出 Tick，一次给一批，调用方直接读这块内存，不再拷贝。
// 读文件用的是 8MB 大小的缓冲，每次读满一整块再逐条分发，避免每读一条就进一次系统调用。
class TickReader final
{
public:
    explicit TickReader(const std::string& path, std::size_t buffer_bytes = std::size_t{8} << 20);
    ~TickReader();

    TickReader(const TickReader&) = delete;
    TickReader& operator=(const TickReader&) = delete;

    // 取下一批可用的 tick。返回 false 表示文件已经读完。
    // 输出指针只在下次调用之前有效，指向读缓冲内部，不复制数据。
    bool NextBatch(const Tick*& ticks, std::size_t& count);

    [[nodiscard]] std::uint64_t TotalRead() const noexcept { return total_read_; }

private:
    bool Fill();

    std::FILE* file_{nullptr};
    std::vector<Tick> buffer_;        // 定长读缓冲，按整条 Tick 计
    std::size_t buffered_ticks_{0};   // 缓冲里现有多少条完整 tick
    std::size_t cursor_{0};           // 下一条要发出去的 tick 下标
    std::size_t leftover_bytes_{0};   // 上一块末尾不足一条 tick 的残留字节
    std::uint64_t total_read_{0};
};

}  // namespace marketflow