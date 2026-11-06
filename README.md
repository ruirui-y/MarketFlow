# MarketFlow

C++20 低延迟行情接入与窗口聚合服务。

单机目标：**10 万 ticks/s 摄入**，分钟级窗口聚合结果与暴力重算 **100% 一致**（逐条比对，0 处偏差）。

---

## 当前进度

| 阶段 | 内容 | 状态 |
|---|---|---|
| M1 | 高吞吐 tick 摄入 + 分钟窗口聚合 | 脚手架 |
| M2 | 订阅推送（按合约过滤，慢消费者隔离） | 未开始 |
| M3 | 订单账本 + 资金守恒校验 | 未开始 |

> 本仓库当前只有构建骨架，M1 的数据通路尚未接入。

---

## 目录

```text
MarketFlow/
├── marketflow/             库：行情模型、二进制读取、窗口聚合（头文件与实现同目录）
│   ├── model/              Tick / Bar 数据结构
│   ├── io/                 二进制文件读取
│   └── aggregate/          窗口聚合
├── app/                    主程序
├── tests/                  正确性测试（暴力重算对照）
├── tools/                  数据生成脚本
├── data/                   数据目录（大文件不入库）
├── docs/                   设计与实验记录
└── perf-results/           每次运行的原始数据（不入库）
```

---

## 构建

Linux（Ninja）：

```bash
cmake --preset linux-debug
cmake --build --preset build-linux-debug
ctest --preset test-linux-debug
```

Release：

```bash
cmake --preset linux-release
cmake --build --preset build-linux-release
```

Windows：CMakeLists 不限定平台，M1 只用标准库，可直接用 Visual Studio 的 CMake 工程打开构建。

---

## 数据准备

仓库不保存行情数据。按 `data/README.md` 生成 `data/ticks.bin`，然后：

```bash
./build/linux-release/bin/marketflow data/ticks.bin
```

---

## 设计要点（四条规则，不接受随意更改）

| 规则 | 约定 |
|---|---|
| 窗口边界 | `window_start = ts_ns / 60000000000 * 60000000000`，向下取整，不重叠 |
| 迟到 tick | 允许 watermark 回退 3 秒；超出范围丢弃并计数，丢弃率必须写入报告 |
| 空窗口 | 无成交的分钟不生成 bar |
| 价格类型 | `int64`，单位分。禁止用 double 累计成交额 |

---

## 证据标准

每个阶段必须产出三份文件，缺一件视为未完成：

| 文件 | 内容 |
|---|---|
| `perf-results/<round>/result.csv` | 逐次运行的原始数据 |
| `perf-results/<round>/environment.txt` | 机器规格、编译选项、参数、时间 |
| `docs/<round>.md` | 结论、对照组、正确性校验结果 |

性能结论必须在 `environment.txt` 可复现的前提下才允许写进文档。