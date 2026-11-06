#include "marketflow/aggregate/bar_aggregator.h"

namespace marketflow
{

BarAggregator::BarAggregator(std::int64_t allowed_late_ns)
    : allowed_late_ns_(allowed_late_ns)
{
}

// 水位线 = 已见最大时间戳 - 允许乱序的余量。它只会往前走，不会往回退。
std::uint64_t BarAggregator::Watermark() const noexcept
{
    const std::int64_t watermark = static_cast<std::int64_t>(max_ts_ns_) - allowed_late_ns_;
    return watermark < 0 ? 0 : static_cast<std::uint64_t>(watermark);
}

bool BarAggregator::OnTick(const Tick& tick)
{
    const std::uint64_t ts = tick.ts_ns;
    const std::uint64_t window_start = WindowStart(ts);

    // 窗口在它结束时刻到达水位线时就已经关闭并输出过；此后再来的成交不能再并进去。
    if (has_any_tick_ && window_start + kWindowNs <= Watermark())
    {
        ++late_dropped_ticks_;
        return false;
    }

    const std::uint64_t key = MakeBarKey(tick.symbol_id, window_start / kWindowNs);

    auto it = open_bars_.find(key);
    if (it == open_bars_.end())
    {
        // 这个窗口第一次出现：四个价格字段都先等于第一笔成交价。
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
        close_heap_.push(CloseAt{window_start + kWindowNs, key});
    }
    else
    {
        // close 一直跟着最新一笔走；open 保持窗口里第一笔到达的成交价。
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

    if (!has_any_tick_ || ts > max_ts_ns_)
    {
        max_ts_ns_ = ts;
        has_any_tick_ = true;
    }

    ++accepted_ticks_;
    return true;
}

// 把结束时刻不晚于 up_to_ns 的窗口依次输出并从内存里摘掉。
void BarAggregator::PopClosed(std::uint64_t up_to_ns, const BarSink& sink)
{
    while (!close_heap_.empty() && close_heap_.top().close_ns <= up_to_ns)
    {
        const CloseAt top = close_heap_.top();
        close_heap_.pop();

        auto it = open_bars_.find(top.key);
        if (it == open_bars_.end())
        {
            // 理论上不会发生；窗口只会从堆里关一次，这里只是防御。
            continue;
        }

        sink(it->second);
        open_bars_.erase(it);
        ++closed_bars_;
    }
}

void BarAggregator::Drain(const BarSink& sink)
{
    if (!has_any_tick_)
    {
        return;
    }

    PopClosed(Watermark(), sink);
}

void BarAggregator::Flush(const BarSink& sink)
{
    // 回放结束时水位线不再前进，这里用最大时间戳兜底，把所有还开着的窗口一次输出。
    PopClosed(UINT64_MAX, sink);
}

}  // namespace marketflow