# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 仓库布局

此仓库根目录是 STM32F411 固件工程的容器，包含两个独立但联动的固件：

| 目录 | 内容 | 何时进入 |
|---|---|---|
| `00_Bootloader/` | Bootloader（StdPeriph，无 RTOS，已从 MDK 移植到 Makefile）| 改 OTA 链路 / 烧录顺序 / Flash 分配 |
| `01_APP/` | 主应用（HAL + FreeRTOS + LVGL + 多传感器，开发主战场）| 改业务代码、传感器、UI |
| `lancedb/` | 工具/索引数据，非固件源码 | — |

两个子工程都自带更详细的 `CLAUDE.md`。所有 `make` 命令必须在子工程目录下运行（不是仓库根目录）。

## 内部 Flash 分配

Bootloader、Flag 区、APP 共享 STM32F411CE 的 512 KB 片上 Flash。地址范围**必须**和 ld 脚本、`bootmanager.h` 中的常量保持一致，否则 Bootloader 跳转后会跑到错误的 vector table 上：

| 区段 | 起止地址 | 大小 | Sector | 内容 |
|---|---|---|---|---|
| Bootloader | `0x08000000 – 0x08007FFF` | 32 KB | 0, 1 | `00_Bootloader/build/bootloader.hex`，上电直跳 APP |
| Flag | `0x08008000 – 0x0800BFFF` | 16 KB | 2 | 预留 OTA 状态/版本标志（暂未使用，OTA 备份走外部 W25Q64）|
| APP | `0x0800C000 – 0x0807FFFF` | 464 KB | 3–7 | `01_APP/build/helloworld.hex` |

锁定这套分配的关键文件：

- `00_Bootloader/STM32F411XX_BOOTLOADER_FLASH.ld` — `FLASH ORIGIN = 0x08000000, LENGTH = 32K`
- `00_Bootloader/Tasks/Bootmanager/inc/bootmanager.h` — `AppAddress = 0x0800C000`, `FlagAddress = 0x08008000`
- `01_APP/STM32F411XX_FLASH.ld` — `FLASH ORIGIN = 0x0800C000, LENGTH = 464K`
- `01_APP/Core/Src/system_stm32f4xx.c` — `USER_VECT_TAB_ADDRESS` 启用，`VECT_TAB_OFFSET = 0x0000C000`（APP 主动 set `SCB->VTOR`，防御性）

改动任何一处都要同步检查这五个位置，否则跳转后 SP 校验会失败、Bootloader 落到 "APP slot invalid" 死循环。

## 内存 / RTT 布局

为了让 J-Link RTT Viewer 同时看到 Bootloader 和 APP 的日志，**两个镜像的 `_SEGGER_RTT` 控制块必须落在同一物理 RAM 地址**：

| 区段 | 地址 | 大小 |
|---|---|---|
| RAM (Bootloader & APP) | `0x20000000` | 121 KB |
| RTT_RAM | `0x2001E400` | 7 KB |

否则若 RTT Viewer 先在 APP 阶段锁定 `0x2001E400`，Bootloader 把 CB 放在别的地址就再也看不到 Bootloader 的日志。Bootloader 通过 `Middlewares/RTT/SEGGER_RTT_Conf.h` 覆盖 `SEGGER_RTT_SECTION = ".RW_RTT"`、并在 ld 里同时匹配 `*(.RW_RTT)` + `*(RTT_BUFFER)`（SEGGER 上游把 ring buffer 硬编码在 `RTT_BUFFER` 段，覆盖不掉）来实现对齐。两镜像 `BUFFER_SIZE_UP` 现统一为 2048 B；上行缓冲只需覆盖一次 host 轮询间隔的突发量，满了丢行不阻塞。RTT_RAM 占用：APP = CB 168 + 上行 2048 + 下行 16 + SystemView 4096 = 6328 B / 7168 B；Bootloader = 168 + 2048 + 16 = 2232 B。改 `BUFFER_SIZE_UP` 或 RTT_RAM 边界时两镜像（及两份 ld）必须同步，否则 CB 错位、共视失效。

注意：APP 的 `elog_port_init()` 调用 `SEGGER_RTT_Init()` 会无条件重置 `WrOff/RdOff`。Bootloader 在 `jump_to_app()` 前 `delay_ms(200)` 给 RTT Viewer 留出轮询时间，否则最后一行 log 会被 APP 抹掉。

## 构建命令

```bash
# Bootloader
cd 00_Bootloader && make            # → build/bootloader.{elf,hex,bin}
cd 00_Bootloader && make -j16
cd 00_Bootloader && make clean
cd 00_Bootloader && make mem-report # Tools/mem_report.py，Flash/RAM/RTT_RAM 三个 region 占用图

# APP
cd 01_APP && make                   # → build/helloworld.{elf,hex,bin,mxxx}
                                    # .mxxx 是 OTA 加密镜像，build-core 自动调
                                    # Tools/ota_encrypt.py 产出（uv run，自动装 pycryptodome）
cd 01_APP && make -j16
cd 01_APP && make clean
cd 01_APP && make mem-report
cd 01_APP && make ota-image         # 单独触发 OTA 镜像生成（默认 all 已包含）
cd 01_APP && make OPT=-O2

# 外部 Flash LVGL 资源（不动 firmware）
cd 01_APP && make pack-assets       # → build/assets.bin
cd 01_APP && make flash-assets      # JFlash CLI + 自定义 .FLM 烧 W25Q64 LVGL 分区
```

工具链：`arm-none-eabi-gcc`，目标 STM32F411xE（Cortex-M4F，硬浮点 fpv4-sp-d16）。`mem-report` / `build_summary` / `ota_encrypt` 全经 `uv run` 走 `Tools/pyproject.toml` 自动管虚拟环境（`uv` 装在 `~/.local/bin`，需登录 shell 才能找到）。

## 烧录顺序

**先烧 APP，再烧 Bootloader**（用户验证过的流程）：

1. JFlash 烧 `01_APP/build/helloworld.hex`（自动落到 `0x0800C000+`）
2. JFlash 烧 `00_Bootloader/build/bootloader.hex`（自动落到 `0x08000000+`）
3. 上电 → Bootloader 进 `OTA_StateManager` 主循环 → 读内部 Flash `0x08008000` 的 ota_flag → 多数情况下 state=0x00（NO_APP_UPDATE）直接 `jump_to_app()` → APP 起来

Bootloader 现在跑完整的 OTA 状态机：clock + SysTick + GPIO + USART1 + SPI2/W25Q64 + elog 初始化后进 `for(;;) OTA_StateManager(); delay_ms(500);` 循环，按 ota_flag 状态决定行为：

- `0xFF INIT_NO_APP` / `0x00 NO_APP_UPDATE`：直接跳 APP（无键按下场景）
- `0x22 DOWNLOAD_FINISHED`：解密 W25Q64 BLOCK_1 → BLOCK_2 → 备份当前 APP → 拷贝到内部 APP 槽 → 写 `0x33`、跳 APP
- `0x33 CHECK_START` / `0x44 CHECKING`：APP confirm + 回滚兜底路径

APP 槽位无效（SP 不在 `0x20000000~0x2001FFFF`）会落 "APP slot invalid" 死循环。完整 OTA 链路细节见 `01_APP/CLAUDE.md` 的 "OTA 升级链路" 节。Bootloader 当前 Flash 占用 ~89% / 32 KB（OTA + AES + Ymodem + SFUD 全 reachable）。

## 调试链路

| 操作 | 工具 |
|---|---|
| 烧固件 | SEGGER JFlash —— Bootloader/APP 分别打开各自 `.hex` |
| 源码调试 | SEGGER Ozone —— 加载对应 `.elf`，通过 JLink 连接 |
| OS 任务追踪（仅 APP） | SEGGER SystemView —— 通过 JLink 附加 |
| printf 日志 | JLink RTT Viewer —— 通道 0，1000000 波特率，CB 地址 `0x2001E400` |
| SWO 输出（仅 APP） | JLink SWO Viewer 或 Ozone SWO 终端 |

## 详细架构

**APP 的完整分层架构、BSP 适配器模式、OSAL 设计、调试日志路由（RTT vs ITM/SWO）、新增外设步骤等，全部记录在 [01_APP/CLAUDE.md](01_APP/CLAUDE.md) 中。** 开始任何 `01_APP/` 内的工作前先读那个文件。

要点速览（细节见上述文件）：

- 严格分层：`01_APP/01_App` / `01_APP/02_Service` → `01_APP/03_Platform`（接口）→ `01_APP/04_Impl`（实现）→ ST HAL / FreeRTOS → CMSIS / 寄存器
- BSP 五段式：`driver/`（裸协议）+ `handler/`（任务）+ `bsp_wrapper_*`（vtable API）+ `bsp_adapter_port_*`（注册）+ `Bsp_Integration/`（input_arg 组装）
- 所有任务集中登记在 `01_APP/01_App/User_Task_Config/src/user_task_reso_config.c` 的 `g_user_task_cfg[]`
- ISR 不可获取 IIC 总线互斥锁——通过 `osal_notify` 唤醒 handler 任务在线程上下文操作
- 日志：`DEBUG_OUT(level, tag, fmt, ...)` 按 tag 路由到 RTT Terminal（0–8）或 ITM/SWO
- LVGL 资源（指针小图 + 240×240 表盘背景）在外部 W25Q64 上：sprite 由 firmware self-bootstrap 从 `.rodata` 写入；240×240 背景太大不进 `.rodata`，必须由 `make flash-assets` 经自定义 .FLM 写入

## VSCode 工作区

推荐打开 **`03_Firmware/stm32-firmware.code-workspace`**（多根 workspace）：把 `Bootloader`、`APP`、`Firmware Root` 列成三个独立 folder，每个 folder 自动使用自己 `.vscode/c_cpp_properties.json` 里的 includePath，**完美回避 APP/Bootloader 同名头文件（`main.h`、`gpio.h`、`tim.h`、`spi.h`、`stm32f4xx_it.h`）的冲突**。

也可以直接打开 `03_Firmware/` 单文件夹模式，靠 `03_Firmware/.vscode/c_cpp_properties.json` 的统一 "Firmware (APP + Bootloader)" 配置——但同名头文件会按 includePath 顺序解析（当前 APP 在前），编辑 Bootloader 文件时 `#include "main.h"` 可能跳到 APP 的 main.h。所以**主战场跨工程时优先用 `.code-workspace`**。

两个子工程目录下 `00_Bootloader/.vscode/` 和 `01_APP/.vscode/` 各自保留独立配置，单开任一子工程作为 workspace 也正常工作。
