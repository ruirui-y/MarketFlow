#include "marketflow/aggregate/bar_aggregator.h"
#include "marketflow/io/tick_reader.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

namespace
{

void PrintUsage()
{
    std::printf("用法: marketflow <ticks.bin> [--csv bars.csv]\n");
    std::printf("  ticks.bin  定长 32 字节的逐笔成交文件，格式见 data/README.md\n");
    std::printf("  --csv      把聚合出的 K 线写成 CSV，便于外部核对\n");
}

bool WriteCsv(const std::string& path, const std::vector<marketflow::Bar>& bars)
{
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr)
    {
        return false;
    }

    std::fprintf(file, "symbol_id,window_start_ns,open,high,low,close,volume,amount\n");
    for (const marketflow::Bar& bar : bars)
    {
        std::fprintf(file, "%u,%llu,%lld,%lld,%lld,%lld,%llu,%llu\n",
                     static_cast<unsigned>(bar.symbol_id),
                     static_cast<unsigned long long>(bar.window_start_ns),
                     static_cast<long long>(bar.open),
                     static_cast<long long>(bar.high),
                     static_cast<long long>(bar.low),
                     static_cast<long long>(bar.close),
                     static_cast<unsigned long long>(bar.volume),
                     static_cast<unsigned long long>(bar.amount));
    }

    std::fclose(file);
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        PrintUsage();
        return 1;
    }

    const std::string input_path = argv[1];
    std::string csv_path;

    for (int i = 2; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc)
        {
            csv_path = argv[++i];
        }
    }

    try
    {
        marketflow::TickReader reader(input_path);
        marketflow::BarAggregator aggregator;

        std::vector<marketflow::Bar> bars;
        const auto sink = [&bars](const marketflow::Bar& bar) { bars.push_back(bar); };

        std::size_t peak_open_windows = 0;

        const auto started = std::chrono::steady_clock::now();

        const marketflow::Tick* ticks = nullptr;
        std::size_t count = 0;
        while (reader.NextBatch(ticks, count))
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                aggregator.OnTick(ticks[i]);
            }

            // 每块结束后推一次水位线，把已经确定不会再变的窗口关掉，避免内存一直涨。
            aggregator.Drain(sink);

            const std::size_t open_windows = aggregator.OpenWindowCount();
            if (open_windows > peak_open_windows)
            {
                peak_open_windows = open_windows;
            }
        }

        aggregator.Flush(sink);

        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        const std::uint64_t total = reader.TotalRead();

        std::printf("tick 总数        : %llu\n", static_cast<unsigned long long>(total));
        std::printf("被接收           : %llu\n", static_cast<unsigned long long>(aggregator.AcceptedTicks()));
        std::printf("迟到丢弃         : %llu\n", static_cast<unsigned long long>(aggregator.LateDroppedTicks()));
        std::printf("K 线根数         : %llu\n", static_cast<unsigned long long>(aggregator.ClosedBars()));
        std::printf("峰值同时开窗     : %zu\n", peak_open_windows);
        std::printf("耗时             : %.3f s\n", elapsed);
        if (elapsed > 0.0)
        {
            std::printf("吞吐             : %.0f ticks/s\n", static_cast<double>(total) / elapsed);
            std::printf("单条平均         : %.1f ns\n", elapsed * 1e9 / static_cast<double>(total));
        }

        if (!csv_path.empty())
        {
            if (WriteCsv(csv_path, bars))
            {
                std::printf("K 线已写入       : %s\n", csv_path.c_str());
            }
            else
            {
                std::printf("K 线写入失败     : %s\n", csv_path.c_str());
                return 1;
            }
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::printf("失败: %s\n", error.what());
        return 1;
    }
}