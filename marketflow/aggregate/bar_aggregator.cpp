#include "marketflow/aggregate/bar_aggregator.h"

namespace marketflow
{

// 允许晚到多少纳秒，还能并进已经出现过的那一分钟。
BarAggregator::BarAggregator(std::int64_t allowed_late_ns)
    : allowed_late_ns_(allowed_late_ns)
{
}

// 算当前界线：收到过的最新一条成交时间，往回退 3 秒。
std::uint64_t BarAggregator::Watermark() const noexcept
{
    const std::int64_t watermark = static_cast<std::int64_t>(max_ts_ns_) - allowed_late_ns_;
    return watermark < 0 ? 0 : static_cast<std::uint64_t>(watermark);
}

// 把一条成交记录并进它那一分钟的 K 线；那一分钟已经交出去过就丢弃并返回 false。
bool BarAggregator::OnTick(const Tick& tick)
{
    const std::uint64_t ts = tick.ts_ns;
    const std::uint64_t window_start = WindowStart(ts);                     // 这条记录属于哪一分钟

    // 这一分钟的结束时刻已经落到界线之前，说明那根 K 线早就交出去了，这条没地方放，只能丢。
    if (has_any_tick_ && window_start + kWindowNs <= Watermark())
    {
        ++late_dropped_ticks_;
        return false;
    }

    const std::uint64_t key = MakeBarKey(tick.symbol_id, window_start / kWindowNs);   // 压成 64 位键，省掉 pair 比较

    auto it = open_bars_.find(key);
    if (it == open_bars_.end())
    {
        // 这一分钟第一次出现：四个价格都先等于第一笔成交价。
        Bar bar{};
        bar.symbol_id = tick.symbol_id;
        bar.window_start_ns = window_start;
        bar.open = tick.price;
        bar.high = tick.price;
        bar.low = tick.price;
        bar.close = tick.price;
        bar.volume = tick.volume;
        bar.amount = static_cast<std::uint64_t>(tick.price) * tick.volume;
        open_bars_.emplace(key, bar);
        close_heap_.push(CloseAt{window_start + kWindowNs, key});           // 结束时刻入堆，决定它何时能被关
    }
    else
    {
        // 不是第一次：跟当前最高最低比一遍，收盘价换成这一笔，成交量累加。
        Bar& bar = it->second;
        if (tick.price > bar.high)
        {
            bar.high = tick.price;
        }
        if (tick.price < bar.low)
        {
            bar.low = tick.price;
        }
        bar.close = tick.price;
        bar.volume += tick.volume;
        bar.amount += static_cast<std::uint64_t>(tick.price) * tick.volume;
    }

    // 只记最大值，界线靠它往后推。
    if (!has_any_tick_ || ts > max_ts_ns_)
    {
        max_ts_ns_ = ts;
        has_any_tick_ = true;
    }

    ++accepted_ticks_;
    return true;
}

// 把结束时刻不晚于 up_to_ns 的分钟依次交出去。
void BarAggregator::PopClosed(std::uint64_t up_to_ns, const BarSink& sink)
{
    // 只有堆顶可能到期；关掉一个，再看下一个。
    while (!close_heap_.empty() && close_heap_.top().close_ns <= up_to_ns)
    {
        const CloseAt top = close_heap_.top();
        close_heap_.pop();

        auto it = open_bars_.find(top.key);
        if (it == open_bars_.end())
        {
            continue;                                                       // 理论上不会发生，防御
        }

        sink(it->second);
        open_bars_.erase(it);
        ++closed_bars_;
    }
}

// 把「已经确定不会再变」的那几分钟交出去。
void BarAggregator::Drain(const BarSink& sink)
{
    if (!has_any_tick_)
    {
        return;
    }

    PopClosed(Watermark(), sink);                                           // 只关界线越过的
}

// 文件读完了，把还开着的分钟全部算完交出去。
void BarAggregator::Flush(const BarSink& sink)
{
    PopClosed(UINT64_MAX, sink);                                            // 用最大值兜底，剩下全关
}

} // namespace marketflow