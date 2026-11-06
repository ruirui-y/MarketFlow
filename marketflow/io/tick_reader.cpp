#include "marketflow/io/tick_reader.h"

#include <cstring>
#include <stdexcept>

namespace marketflow
{

namespace
{
constexpr std::size_t kTickSize = sizeof(Tick);
}

// 打开 ticks.bin，按整条记录对齐准备一块读缓冲。
TickReader::TickReader(const std::string& path, std::size_t buffer_bytes)
{
    file_ = std::fopen(path.c_str(), "rb");                    // "rb" = 二进制只读，b 不能省
    if (file_ == nullptr)
    {
        throw std::runtime_error("无法打开 tick 数据文件: " + path);
    }

    const std::size_t block_ticks = buffer_bytes / kTickSize;
    buffer_.resize(block_ticks == 0 ? 1 : block_ticks);
}

// 关闭文件。
TickReader::~TickReader()
{
    if (file_ != nullptr)
    {
        std::fclose(file_);
        file_ = nullptr;
    }
}

// 把上一块末尾的残缺字节挪到缓冲开头，再在后面继续读满一块。
bool TickReader::Fill()
{
    if (file_ == nullptr)
    {
        return false;
    }

    auto* base = reinterpret_cast<char*>(buffer_.data());

    // 上一轮末尾剩下几个字节，凑不成一条 Tick（不够 32 字节）。
    // 把它们挪到缓冲最前面，这一轮新读的数据接在后面，就能拼成完整记录。
    if (leftover_bytes_ > 0)
    {
        std::memmove(base,                              // 搬到哪：缓冲开头
            base + buffered_ticks_ * kTickSize,         // 从哪搬：所有完整记录之后的残留
            leftover_bytes_);                           // 搬几个字节
    }

    // 缓冲总共能装多少字节，减去已被残留占掉的，才是这一轮能读进来的量。
    const std::size_t capacity = buffer_.size() * kTickSize - leftover_bytes_;

    const std::size_t got = std::fread(base + leftover_bytes_,          // 读到残留数据之后的位置
        1,                                                              // 元素大小：1 字节
        capacity,                                                       // 最多读 capacity 个元素
        file_);                                                         // 从文件读取

    const std::size_t total_bytes = leftover_bytes_ + got;
    buffered_ticks_ = total_bytes / kTickSize;                          // 除 32：能凑成几条完整记录。
    leftover_bytes_ = total_bytes % kTickSize;                          // 取余：凑不满一条的零头（0 到 31 字节），留给下一轮。
    cursor_ = 0;                                                        // 发的时候从头开始
    total_read_ += buffered_ticks_;                                     // 累计读了多少条

    return buffered_ticks_ > 0;                                         // 一条都凑不出来，说明读完了
}

// 取下一批记录。上一批发完了就先读下一块，读不到就说明文件结束。
bool TickReader::NextBatch(const Tick*& ticks, std::size_t& count)
{
    // 上一块发完了，去读下一块。
    if (cursor_ >= buffered_ticks_)
    {
        if (!Fill())
        {
            return false;                                               // 文件也读完了
        }
    }

    ticks = buffer_.data() + cursor_;                                   // 直接指向缓冲内部，不复制
    count = buffered_ticks_ - cursor_;
    cursor_ = buffered_ticks_;

    return true;
}

} // namespace marketflow