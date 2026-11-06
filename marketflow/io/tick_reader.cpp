#include "marketflow/io/tick_reader.h"

#include <cstring>
#include <stdexcept>

namespace marketflow
{

namespace
{
constexpr std::size_t kTickSize = sizeof(Tick);
}

TickReader::TickReader(const std::string& path, std::size_t buffer_bytes)
{
    file_ = std::fopen(path.c_str(), "rb");
    if (file_ == nullptr)
    {
        throw std::runtime_error("无法打开 tick 数据文件: " + path);
    }

    // 缓冲按整条 Tick 对齐，正常情况下块里不会出现半条记录；只有文件末尾可能剩几个字节。
    const std::size_t block_ticks = buffer_bytes / kTickSize;
    buffer_.resize(block_ticks == 0 ? 1 : block_ticks);
}

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

    if (leftover_bytes_ > 0)
    {
        std::memmove(base, base + buffered_ticks_ * kTickSize, leftover_bytes_);
    }

    const std::size_t capacity = buffer_.size() * kTickSize - leftover_bytes_;
    const std::size_t got = std::fread(base + leftover_bytes_, 1, capacity, file_);
    const std::size_t total_bytes = leftover_bytes_ + got;

    buffered_ticks_ = total_bytes / kTickSize;
    leftover_bytes_ = total_bytes % kTickSize;
    cursor_ = 0;
    total_read_ += buffered_ticks_;

    return buffered_ticks_ > 0;
}

bool TickReader::NextBatch(const Tick*& ticks, std::size_t& count)
{
    if (cursor_ >= buffered_ticks_)
    {
        if (!Fill())
        {
            return false;
        }
    }

    ticks = buffer_.data() + cursor_;
    count = buffered_ticks_ - cursor_;
    cursor_ = buffered_ticks_;

    return true;
}

}  // namespace marketflow