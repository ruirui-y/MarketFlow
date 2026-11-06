#pragma once

#include <cstdint>

namespace marketflow
{

// 一条逐笔成交。字段按 8 字节对齐排布，整体固定 32 字节，
// 这样二进制文件可以按 sizeof(Tick) 定长切分，读的时候不需要任何解析。
struct Tick
{
    std::uint32_t symbol_id;      // 标的编号，用连续下标，不存字符串
    std::uint32_t reserved;       // 保留字段，仅用于对齐；将来放序号之类的标记
    std::uint64_t ts_ns;          // 交易所时间戳，纳秒，从 Unix 纪元起算
    std::int64_t price;           // 成交价，单位是分；不用 double，避免累计成交额出现浮点误差
    std::uint32_t volume;         // 成交量，单位是股
    std::uint8_t side;            // 买卖方向：0 未知，1 买盘，2 卖盘
    std::uint8_t padding[3];      // 对齐填充
};

static_assert(sizeof(Tick) == 32, "Tick 必须是 32 字节，二进制文件布局依赖这一点");
static_assert(alignof(Tick) == 8, "Tick 需要按 8 字节对齐");

// 一根分钟 K 线。open 记窗口内第一笔到达的成交价，close 记最后一笔。
struct Bar
{
    std::uint32_t symbol_id;
    std::uint32_t reserved;
    std::uint64_t window_start_ns;  // 这个窗口的起始时刻，一定对齐到整分钟
    std::int64_t open;
    std::int64_t high;
    std::int64_t low;
    std::int64_t close;
    std::uint64_t volume;           // 窗口内成交量合计，单位股
    std::uint64_t amount;           // 窗口内成交额合计，单位「分 乘 股」
};

static_assert(sizeof(Bar) == 64, "Bar 必须是 64 字节");

// 窗口长度固定 1 分钟。改这里等于改全项目的窗口语义，不要在别处另写一份。
inline constexpr std::uint64_t kWindowNs = 60'000'000'000ULL;

// 允许乱序 tick 的最大回退幅度。水位线按这个幅度往回留余量，决定窗口什么时候关闭。
inline constexpr std::int64_t kDefaultAllowedLateNs = 3'000'000'000LL;

// 把（标的，分钟序号）压成一个 64 位 key，避免用 pair 做 map 键时多一次间接访问。
// 分钟序号用 Unix 纪元的分钟数，2026 年约为 2900 万，32 位放得下。
inline std::uint64_t MakeBarKey(std::uint32_t symbol_id, std::uint64_t minute_index) noexcept
{
    return (static_cast<std::uint64_t>(symbol_id) << 32) | static_cast<std::uint32_t>(minute_index);
}

// 窗口起点按分钟向下取整；不足一分钟的部分丢掉，保证窗口不重叠。
inline std::uint64_t WindowStart(std::uint64_t ts_ns) noexcept
{
    return ts_ns / kWindowNs * kWindowNs;
}

}  // namespace marketflow