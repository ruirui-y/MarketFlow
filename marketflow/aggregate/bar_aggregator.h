#pragma once

#include "marketflow/model/tick.h"

#include <cstdint>
#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>

namespace marketflow
{

// 把逐笔成交聚合成分钟 K 线。
//
// 处理乱序靠水位线（watermark）：记住已经见过的最大时间戳，往回留出 allowed_late_ns 的余量，
// 认为这个时刻之前结束的窗口不会再变。只有水位线越过某个窗口的结束时刻，这个窗口才会关闭并输出。
//
// 关键点：一条稍晚到达的成交，只要它所属的窗口还没关闭，仍然会正确地并进那一分钟。
// 只有「窗口已经输出之后」才到的成交会被丢弃并计数，不会偷偷改掉已经发出去的数据。
class BarAggregator final
{
public:
    using BarSink = std::function<void(const Bar&)>;

    explicit BarAggregator(std::int64_t allowed_late_ns = kDefaultAllowedLateNs);

    // 喂一条 tick。返回 false 表示这条被丢弃（所属窗口已经关闭），丢弃次数会被计数。
    bool OnTick(const Tick& tick);

    // 把水位线之前已经关闭的窗口输出。回放过程中按批调用。
    void Drain(const BarSink& sink);

    // 回放结束后调用，把还开着的窗口全部强制输出。
    void Flush(const BarSink& sink);

    [[nodiscard]] std::uint64_t AcceptedTicks() const noexcept { return accepted_ticks_; }
    [[nodiscard]] std::uint64_t LateDroppedTicks() const noexcept { return late_dropped_ticks_; }
    [[nodiscard]] std::uint64_t ClosedBars() const noexcept { return closed_bars_; }
    [[nodiscard]] std::size_t OpenWindowCount() const noexcept { return open_bars_.size(); }

private:
    struct CloseAt
    {
        std::uint64_t close_ns;   // 这个窗口的结束时刻
        std::uint64_t key;

        // 小顶堆：结束时间早的先出堆。
        bool operator>(const CloseAt& other) const noexcept
        {
            if (close_ns != other.close_ns)
            {
                return close_ns > other.close_ns;
            }
            return key > other.key;
        }
    };

    [[nodiscard]] std::uint64_t Watermark() const noexcept;
    void PopClosed(std::uint64_t up_to_ns, const BarSink& sink);

    std::unordered_map<std::uint64_t, Bar> open_bars_;                        // 还没关闭的窗口
    std::priority_queue<CloseAt, std::vector<CloseAt>, std::greater<CloseAt>> close_heap_;  // 按关闭时刻排序
    std::int64_t allowed_late_ns_;
    std::uint64_t max_ts_ns_{0};
    bool has_any_tick_{false};
    std::uint64_t accepted_ticks_{0};
    std::uint64_t late_dropped_ticks_{0};
    std::uint64_t closed_bars_{0};
};

}  // namespace marketflow