#!/usr/bin/env python3
"""生成 M1 用的合成逐笔数据 ticks.bin。

真实的逐笔数据不好批量拿到，M1 先用合成数据把吞吐和正确性跑通：
价格随机游走，时间戳按整分钟均匀铺开，同一组参数永远生成同一份数据。

用法：
    python tools/gen_ticks.py --symbols 50 --minutes 240 --ticks-per-minute 200 --out data/ticks.bin
"""

import argparse
import struct

# 32 字节，与 include/marketflow/model/tick.h 里的 Tick 一一对应：
# symbol_id(u32) reserved(u32) ts_ns(u64) price(i64) volume(u32) side(u8) padding(3)
TICK = struct.Struct("<IIQqIB3x")
WINDOW_NS = 60_000_000_000
# 基准时间对齐到整分钟，保证 --minutes 生成的时间窗数量与参数一致。
BASE_TS = (1_760_000_000 // 60) * 60 * 1_000_000_000
FLUSH_BYTES = 8 << 20


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--symbols", type=int, default=50)
    parser.add_argument("--minutes", type=int, default=240)
    parser.add_argument("--ticks-per-minute", type=int, default=200)
    parser.add_argument("--out", default="data/ticks.bin")
    parser.add_argument("--seed", type=int, default=20261106)
    args = parser.parse_args()

    if args.ticks_per_minute <= 0:
        raise SystemExit("--ticks-per-minute 必须大于 0")

    import random
    rng = random.Random(args.seed)

    step_ns = WINDOW_NS // args.ticks_per_minute
    written = 0
    buf = bytearray()

    with open(args.out, "wb") as file:
        for minute in range(args.minutes):
            base = BASE_TS + minute * WINDOW_NS
            for i in range(args.ticks_per_minute):
                ts = base + i * step_ns
                for symbol in range(args.symbols):
                    price = 1000 + symbol * 37 + rng.randint(-3, 3)
                    volume = rng.randint(1, 500)
                    side = 1 + (i & 1)
                    buf += TICK.pack(symbol, 0, ts, price, volume, side)
                    written += 1
                    if len(buf) >= FLUSH_BYTES:
                        file.write(buf)
                        buf.clear()
        if buf:
            file.write(buf)

    print(f"已写出 {args.out}：{written} 条，{written * TICK.size / 1024 / 1024:.1f} MiB")


if __name__ == "__main__":
    main()