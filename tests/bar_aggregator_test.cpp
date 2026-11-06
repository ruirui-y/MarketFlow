#include "marketflow/aggregate/bar_aggregator.h"
#include "marketflow/model/tick.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <random>
#include <vector>

using namespace marketflow;

namespace
{

bool g_passed = true;

void Check(bool condition, const char* what)
{
    std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
    if (!condition)
    {
        g_passed = false;
    }
}

// 基准时间必须先对齐到整分钟，否则窗口切分会整体偏几十秒，测试就失去意义。
constexpr std::uint64_t kBaseTs = (1'760'000'000ULL / 60ULL) * 60ULL * 1'000'000'000ULL;

// 暴力版本：不用水位线、不关窗口，把所有 tick 直接塞进 std::map。
// 它和 BarAggregator 是两套独立写法，用来做对照。
std::map<std::uint64_t, Bar> BuildReference(const std::vector<Tick>& ticks)
{
    std::map<std::uint64_t, Bar> bars;

    for (const Tick& tick : ticks)
    {
        const std::uint64_t window_start = WindowStart(tick.ts_ns);
        const std::uint64_t key = MakeBarKey(tick.symbol_id, window_start / kWindowNs);

        auto it = bars.find(key);
        if (it == bars.end())
        {
            Bar bar{};
            bar.symbol_id = tick.symbol_id;
            bar.window_start_ns = window_start;
            bar.open = tick.price;
            bar.high = tick.price;
            bar.low = tick.price;
            bar.close = tick.price;
            bar.volume = tick.volume;
            bar.amount = static_cast<std::uint64_t>(tick.price) * tick.volume;
            bars.emplace(key, bar);
            continue;
        }

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

    return bars;
}

std::vector<Tick> MakeTicks(std::uint32_t symbol_count, std::uint32_t minutes, std::uint32_t per_minute, unsigned seed)
{
    std::vector<Tick> ticks;
    ticks.reserve(static_cast<std::size_t>(symbol_count) * minutes * per_minute);

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> price_step(-3, 3);
    std::uniform_int_distribution<std::uint32_t> volume_dist(1, 500);

    for (std::uint32_t symbol = 0; symbol < symbol_count; ++symbol)
    {
        std::int64_t price = 1000 + static_cast<std::int64_t>(symbol) * 37;

        for (std::uint32_t minute = 0; minute < minutes; ++minute)
        {
            for (std::uint32_t i = 0; i < per_minute; ++i)
            {
                price += price_step(rng);
                if (price < 1)
                {
                    price = 1;
                }

                Tick tick{};
                tick.symbol_id = symbol;
                tick.ts_ns = kBaseTs + static_cast<std::uint64_t>(minute) * kWindowNs +
                             static_cast<std::uint64_t>(i) * (kWindowNs / per_minute);
                tick.price = price;
                tick.volume = volume_dist(rng);
                tick.side = static_cast<std::uint8_t>(1 + (i & 1));
                ticks.push_back(tick);
            }
        }
    }

    return ticks;
}

void TestStreamingMatchesReference()
{
    std::printf("[case 1] 流式聚合与暴力重算逐条一致\n");

    std::vector<Tick> ticks = MakeTicks(20, 60, 50, 20261106);
    std::sort(ticks.begin(), ticks.end(), [](const Tick& a, const Tick& b)
        {
            return a.ts_ns < b.ts_ns;
        });

    BarAggregator aggregator;
    std::map<std::uint64_t, Bar> streamed;

    for (const Tick& tick : ticks)
    {
        aggregator.OnTick(tick);
    }
    aggregator.Flush([&streamed](const Bar& bar)
        {
            streamed.emplace(MakeBarKey(bar.symbol_id, bar.window_start_ns / kWindowNs), bar);
        });

    const std::map<std::uint64_t, Bar> reference = BuildReference(ticks);

    Check(streamed.size() == reference.size(), "bar 数量一致");

    bool same = true;
    for (const auto& entry : reference)
    {
        const auto it = streamed.find(entry.first);
        if (it == streamed.end())
        {
            same = false;
            break;
        }

        const Bar& actual = it->second;
        const Bar& expected = entry.second;
        if (actual.open != expected.open || actual.high != expected.high || actual.low != expected.low ||
            actual.close != expected.close || actual.volume != expected.volume || actual.amount != expected.amount ||
            actual.window_start_ns != expected.window_start_ns)
        {
            same = false;
            break;
        }
    }
    Check(same, "每根 K 线的 open/high/low/close/volume/amount 全部一致");
    Check(aggregator.LateDroppedTicks() == 0, "顺序数据不产生丢弃");
}

void TestLateTickIsDropped()
{
    std::printf("[case 2] 窗口关闭后的迟到 tick 被丢弃\n");

    BarAggregator aggregator;
    std::uint32_t bar_count = 0;
    const auto sink = [&bar_count](const Bar&) { ++bar_count; };

    for (int i = 0; i < 3; ++i)
    {
        Tick tick{};
        tick.symbol_id = 1;
        tick.ts_ns = kBaseTs + static_cast<std::uint64_t>(i) * 1'000'000'000ULL;
        tick.price = 100 + i;
        tick.volume = 10;
        aggregator.OnTick(tick);
    }

    Tick far_tick{};
    far_tick.symbol_id = 1;
    far_tick.ts_ns = kBaseTs + 3 * kWindowNs;
    far_tick.price = 200;
    far_tick.volume = 10;
    aggregator.OnTick(far_tick);

    aggregator.Drain(sink);
    Check(bar_count == 1, "水位线越过第 0 分钟后该窗口被关闭输出");

    Tick late_tick{};
    late_tick.symbol_id = 1;
    late_tick.ts_ns = kBaseTs + 30'000'000'000ULL;   // 落在第 0 分钟，但窗口已经关了
    late_tick.price = 999;
    late_tick.volume = 10;

    const bool accepted = aggregator.OnTick(late_tick);
    Check(!accepted, "窗口关闭后的迟到 tick 被拒绝");
    Check(aggregator.LateDroppedTicks() == 1, "迟到计数为 1");
}

void TestWindowBoundary()
{
    std::printf("[case 3] 窗口边界按整分钟切分\n");

    BarAggregator aggregator;
    std::vector<Bar> bars;

    Tick a{}; a.symbol_id = 7; a.ts_ns = kBaseTs + kWindowNs - 1; a.price = 10; a.volume = 1;
    Tick b{}; b.symbol_id = 7; b.ts_ns = kBaseTs + kWindowNs;     b.price = 20; b.volume = 1;
    Tick c{}; c.symbol_id = 7; c.ts_ns = kBaseTs + 5;             c.price = 30; c.volume = 1;

    aggregator.OnTick(a);
    aggregator.OnTick(b);
    aggregator.OnTick(c);

    aggregator.Flush([&bars](const Bar& bar) { bars.push_back(bar); });

    Check(bars.size() == 2, "跨越整分钟的成交落在两根 K 线里");
    if (bars.size() == 2)
    {
        Check(bars[0].window_start_ns == kBaseTs, "第一根 K 线起点是整分钟");
        Check(bars[0].open == 10 && bars[0].high == 30 && bars[0].low == 10 && bars[0].close == 30,
              "第一根 K 线的四个价格正确（open 取窗口内第一笔到达价）");
        Check(bars[0].volume == 2 && bars[0].amount == 40, "第一根 K 线的成交量与成交额正确");
        Check(bars[1].open == 20 && bars[1].close == 20, "第二根 K 线只有一笔成交");
    }
}

}  // namespace

int main()
{
    TestStreamingMatchesReference();
    TestLateTickIsDropped();
    TestWindowBoundary();

    if (!g_passed)
    {
        std::printf("\n有断言失败\n");
        return 1;
    }

    std::printf("\n全部通过\n");
    return 0;
}