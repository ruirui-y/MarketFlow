#pragma once

#include "marketflow/model/tick.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace marketflow
{

// 从 ticks.bin 按块读成交记录。一次给一批，调用方直接读这块内存，不复制。
class TickReader final
{
public:
    explicit TickReader(const std::string& path, std::size_t buffer_bytes = std::size_t{8} << 20);
    ~TickReader();

    TickReader(const TickReader&) = delete;
    TickReader& operator=(const TickReader&) = delete;

    // 取下一批。返回 false 表示文件读完。
    // 输出指针指向读缓冲内部，只在下次调用之前有效。
    bool NextBatch(const Tick*& ticks, std::size_t& count);

    [[nodiscard]] std::uint64_t TotalRead() const noexcept { return total_read_; }

private:
    bool Fill();

    std::FILE* file_{nullptr};
    std::vector<Tick> buffer_;                                                                  // 定长读缓冲，按整条算
    std::size_t buffered_ticks_{0};                                                             // 缓冲里现在有多少条完整的
    std::size_t cursor_{0};                                                                     // 下一条要发出去的位置
    std::size_t leftover_bytes_{0};                                                             // 上一块末尾凑不满一条的残留字节
    std::uint64_t total_read_{0};                                                               // 累计读过多少条
};

} // namespace marketflow