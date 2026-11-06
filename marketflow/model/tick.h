#pragma once

#include <cstdint>

namespace marketflow
{

// 术语：tick = 一条成交记录；bar = 一分钟 K 线。
// 一条成交记录：什么时间、什么价、成交多少。整体固定 32 字节，
// 二进制文件按 sizeof(Tick) 定长切分，读的时候不用解析。
struct Tick
{
    std::uint32_t symbol_id;                                                                    // 标的编号，用连续下标，不存字符串
    std::uint32_t reserved;                                                                     // 对齐填充；将来放序号之类的标记
    std::uint64_t ts_ns;                                                                        // 纳秒，从 Unix 纪元起算
    std::int64_t price;                                                                         // 单位「分」；不用 double，累加金额会飘
    std::uint32_t volume;                                                                       // 单位股
    std::uint8_t side;                                                                          // 0 未知 / 1 买盘 / 2 卖盘
    std::uint8_t padding[3];                                                                    // 凑齐 32 字节
};

static_assert(sizeof(Tick) == 32, "二进制文件布局依赖 32 字节");
static_assert(alignof(Tick) == 8, "按 8 字节对齐，直接读缓冲才安全");

// 一根分钟 K 线：记开盘、最高、最低、收盘和这一分钟的总量。
struct Bar
{
    std::uint32_t symbol_id;
    std::uint32_t reserved;
    std::uint64_t window_start_ns;                                                              // 一定对齐到整分钟
    std::int64_t open;
    std::int64_t high;
    std::int64_t low;
    std::int64_t close;
    std::uint64_t volume;                                                                       // 这一分钟的成交量合计，单位股
    std::uint64_t amount;                                                                       // 这一分钟的成交额合计，单位「分 乘 股」
};

static_assert(sizeof(Bar) == 64, "Bar 固定 64 字节");

// 一分钟有多长。改这里等于改全项目的窗口语义，不要在别处另写一份。
inline constexpr std::uint64_t kWindowNs = 60'000'000'000ULL;

// 允许乱序的幅度：数据最多晚到多久还能收。界线按这个值往回留余量。
inline constexpr std::int64_t kDefaultAllowedLateNs = 3'000'000'000LL;

// 把（标的，分钟序号）压成一个 64 位键，避免拿 pair 做 map 键时多一次间接访问。
inline std::uint64_t MakeBarKey(std::uint32_t symbol_id, std::uint64_t minute_index) noexcept
{
    return (static_cast<std::uint64_t>(symbol_id) << 32) | static_cast<std::uint32_t>(minute_index);
}

// 这一分钟从哪一刻开始：时间戳向下取整到整分钟。
inline std::uint64_t WindowStart(std::uint64_t ts_ns) noexcept
{
    return ts_ns / kWindowNs * kWindowNs;
}

} // namespace marketflow