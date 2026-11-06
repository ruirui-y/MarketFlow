# 数据目录

本目录只放数据格式说明，不放数据文件。`*.bin` 与 `*.csv` 均被 `.gitignore` 排除。

## ticks.bin 格式

定长记录，小端序，每条 `sizeof(Tick)` = 32 字节：

| 偏移 | 类型 | 说明 |
|---|---|---|
| 0 | uint32 | symbol_id |
| 4 | uint32 | 对齐填充 |
| 8 | uint64 | ts_ns（纳秒时间戳） |
| 16 | int64 | price（单位：分） |
| 24 | uint32 | volume |
| 28 | uint8 | side（0 未知 / 1 买 / 2 卖） |
| 29 | uint8[3] | 对齐填充 |

## 如何生成

待 M1 的数据导入工具完成后补充命令。