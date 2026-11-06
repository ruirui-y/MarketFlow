#pragma once

#include "marketflow/model/tick.h"

#include <cstdint>
#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>

namespace marketflow
{

// 术语：窗口 = 一分钟；界线 = 最新成交时间往回退 3 秒。
// 把一条条成交记录攒成一根根分钟 K 线：所属那一分钟只要还没越过界线就还能并进去，
// 越过了说明那根 K 线已经交出去，这条记录只能丢。
class BarAggregator final
{
public:
    using BarSink = std::function<void(const Bar&)>;                                            // 一根 K 线算完时的回调

    explicit BarAggregator(std::int64_t allowed_late_ns = kDefaultAllowedLateNs);

    bool OnTick(const Tick& tick);                                                              // 并进它那一分钟的 K 线；那一分钟已交出去则返回 false 并丢弃
    void Drain(const BarSink& sink);                                                            // 把已经确定不会再变的那几分钟交出去
    void Flush(const BarSink& sink);                                                            // 文件读完了，把还开着的分钟全部算完交出去

    [[nodiscard]] std::uint64_t AcceptedTicks() const noexcept { return accepted_ticks_; }
    [[nodiscard]] std::uint64_t LateDroppedTicks() const noexcept { return late_dropped_ticks_; }
    [[nodiscard]] std::uint64_t ClosedBars() const noexcept { return closed_bars_; }
    [[nodiscard]] std::size_t OpenWindowCount() const noexcept { return open_bars_.size(); }

private:
    struct CloseAt
    {
        std::uint64_t close_ns;                                                                 // 这一分钟的结束时刻
        std::uint64_t key;                                                                      // 对应 open_bars_ 里的键

        // std::greater 拿这个比较，效果是小顶堆：结束时刻早的先出堆。
        bool operator>(const CloseAt& other) const noexcept
        {
            if (close_ns != other.close_ns)
            {
                return close_ns > other.close_ns;
            }
            return key > other.key;
        }
    };

    [[nodiscard]] std::uint64_t Watermark() const noexcept;                                     // 算当前界线：最后一条成交的时间戳减去允许的延迟
    void PopClosed(std::uint64_t up_to_ns, const BarSink& sink);                                // 把结束时刻不晚于 up_to_ns 的分钟依次交出去

    // 下面这些状态只在 OnTick / Drain 里改写，没有加锁：一个聚合器只在一个线程里跑。
    std::unordered_map<std::uint64_t, Bar> open_bars_;                                          // 还没算完的分钟，值是那根没算完的 K 线
    std::priority_queue<CloseAt, std::vector<CloseAt>, std::greater<CloseAt>> close_heap_;      // 上面这些分钟按结束时刻排的小顶堆

    std::int64_t allowed_late_ns_;                                                              // 允许乱序多少纳秒，默认 3 秒
    std::uint64_t max_ts_ns_{0};                                                                // 收到过的最新一条成交记录的时间，单位纳秒
    bool has_any_tick_{false};                                                                  // 有没有收到过第一条；没收到时不能算界线
    std::uint64_t accepted_ticks_{0};                                                           // 成功并进去的成交记录条数
    std::uint64_t late_dropped_ticks_{0};                                                       // 因为那一分钟已交出去而丢弃的条数
    std::uint64_t closed_bars_{0};                                                              // 已经算完并交出去的 K 线根数
};

} // namespace marketflow