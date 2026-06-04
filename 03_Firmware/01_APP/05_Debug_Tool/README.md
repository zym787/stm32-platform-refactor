# 05_Debug_Tool — 调试 / 日志 / 追踪

集中放调试期工具（日志、追踪、保护），统一通过 `DEBUG_OUT(level, tag, fmt, ...)` 宏输出，按 tag 路由到不同物理通道。

完整 RTT 通道分配 + 新增 tag 步骤见 [`../CLAUDE.md`](../CLAUDE.md) "调试日志路由"节。

## 子模块

| 子目录 | 内容 |
|---|---|
| `Debug_Log/` | `Debug.h` 定义 `DEBUG_OUT` 宏 + tag 常量；`Debug.c` 实现 tag 过滤 + RTT/ITM 路由；底层走 EasyLogger（[`../04_Impl/impl_middleware/EasyLogger/`](../04_Impl/impl_middleware/EasyLogger/)） |
| `Systemview/` | SEGGER SystemView + RTT 控制块（`SEGGER_RTT.h/.c`）。RTT_RAM 段独立放在 `0x2001E400`，7 KB（上行 buffer `BUFFER_SIZE_UP`=2048）|
| `SWO_Trace/` | `itm_trace.h` —— ITM stimulus port 0 输出，给 ITM-only tag 走 `printf` → SWO Viewer / Ozone SWO 终端 |
| `MPU_Protect/` | `mpu.h` —— MPU region 配置（栈溢出 / 空指针解引用防护） |

## 两路输出共存

```
DEBUG_OUT(level, tag, ...)
   │
   ├── 一般 tag → EasyLogger → SEGGER_RTT_SetTerminal() → RTT 通道 0  → RTT Viewer
   │                                                       (按 Terminal Tab 分组 0-8)
   └── ITM-only tag (route=DEBUG_ROUTE_ITM) → printf → __io_putchar() → ITM port 0 → SWO Viewer
```

RTT Terminal 分组（`DEBUG_RTT_CH_*`）：

| Terminal | 覆盖 tag |
|---|---|
| 0 | 默认（未显式路由） |
| 1 | AHT21 / 温湿度 |
| 2 | WT588 handler / 测试 |
| 3 | MPU6050 / 数据解析 |
| 4 | ST7789 TFT-LCD |
| 5 | CST816T 触摸 |
| 6 | W25Q64 SPI NOR |
| 7 | EM7028 PPG 心率 |
| 8 | 栈水位监控 |

## 新增 tag

tag 路由由 `Debug.c` 的 `s_route_table[]` 单表驱动，`debug_route_lookup()` 一趟扫描定路由；表里没有的 tag 自动丢弃。

1. `Debug.h` 中定义 `*_LOG_TAG` 常量
2. 在 `s_route_table[]` 加一行 `{ TAG, DEBUG_RTT_CH_x, DEBUG_ROUTE_RTT }`（`DEBUG_RTT_CH_DEFAULT` 即终端 0）

ITM-only tag：定义 `*_ITM_LOG_TAG` 后加一行 `{ TAG, 0, DEBUG_ROUTE_ITM }`，无需改 `elog_port.c`。
停用某 tag：删除/注释该行即可。

## 注意

- `elog_port_init()` 调 `SEGGER_RTT_Init()` 会无条件重置 `WrOff/RdOff` —— Bootloader 在 `jump_to_app()` 前 `delay_ms(200)` 留给 RTT Viewer 轮询，否则最后一行日志被 APP 抹掉。
- `_SEGGER_RTT` 控制块固定在 `0x2001E400`，APP 与 Bootloader 共享同一物理地址才能让 RTT Viewer 不切换（改地址需同步两份 ld）。
