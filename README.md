# 智能手表 — 平台化嵌入式固件框架

以 **APP 为核心、四个能力平台为环** 的可移植嵌入式固件架构。业务层只面向抽象 API 编程，底层 MCU、RTOS、外设、中间件均可独立替换，不影响应用代码。

A portable embedded firmware architecture organized around an application core surrounded by four capability platforms. The application layer programs against abstract APIs only; the underlying MCU, RTOS, peripherals, and middleware are independently replaceable without touching application code.

---

## 架构总览

```
                ┌───────────────── OS Platform ─────────────────┐
                │  OSAL_Timer  OSAL_Task  OSAL_Sema/Mutex/Queue │
                │           OSAL_Heap   (时基/线程/IPC/内存)    │
                └────────────────────────────────────────────────┘
                                       │
   ┌──────────────┐                    ▼                  ┌──────────────────┐
   │ MCU Platform │ ◀──────────────  APP  ──────────────▶ │ Middleware Plat. │
   │ Main / Fault │                    ▲                  │ Shell / Math /   │
   │ SVC / PendSV │                    │                  │ Ringbuf / Crypto │
   │ IIC/SPI/UART │                    │                  │ Algo / GUI       │
   └──────────────┘                    │                  └──────────────────┘
                ┌───────────────── BSP Platform ────────────────┐
                │  drv_adapter_<dev>  →  vtable wrapper API     │
                │  温湿度 / 屏幕 / 触摸 / 运动 / 心率 / 存储…   │
                └────────────────────────────────────────────────┘
```

**核心思想**：APP 不感知具体硬件、不感知具体 RTOS、不感知具体中间件实现，只通过四个平台的对外接口调用能力。业务无关的 service（OTA、存储门面等）以 `01_SERVICE/` 落地，与 APP 平级，依然只调下层平台。

| 平台 | 抽象接口 | 当前实现 | 替换粒度 |
|---|---|---|---|
| **OS Platform** | OSAL（任务、互斥、信号量、队列、定时器、堆） | FreeRTOS 映射 | 替换 `OS_Implementation` 即可换 RTOS |
| **MCU Platform** | I2C/SPI/UART 总线接口 + 启动/异常/上下文切换 | STM32 HAL + 软件位操作 | 替换 port 层即可换 MCU |
| **BSP Platform** | 设备类别 vtable（温湿度/显示/触摸/运动/存储…） | aht21、mpu6050、st7789、cst816t、wt588f02、w25q64 | 同类设备直接换 driver+adapter |
| **Middleware Platform** | 命令行/数据处理/缓冲/密码学/算法/图形等 | LetterShell、LVGL、EasyLogger… | 中间件之间互不耦合，按需启停 |

详细分层与 BSP 适配器五段式拆分见 [固件架构与开发指南](03_Firmware/01_APP/README.md)。

---

## 目录结构

```
00_Reference/       数据手册与参考文档
01_Function_Map/    功能规划
02_Hardware/        硬件设计（原理图、PCB）
03_Firmware/        固件工程（平台化主体，详见内部 README）
04_Software/        上位机软件
05_Tools/           辅助工具
```

固件本体位于 [03_Firmware/01_APP/](03_Firmware/01_APP/)，目录命名直接反映平台分层：

```
01_APP/                   业务逻辑、任务表、IO 注册、ISR 派发
01_SERVICE/               业务无关 service（OTA 升级链路、storage 异步门面）
02_OS_Platform/           OSAL 封装 + RTOS 实现映射
02_MCU_Platform/          I2C / SPI / UART / IFlash / Watchdog 总线 port + 互斥锁
02_BSP_Platform/          driver / handler / wrapper / adapter / integration 五段式
02_Middleware_Platform/   Log / Ymodem / LVGL / 心率算法（LetterShell 已停用）
03_Config/                CFG_ 前缀的项目级开关（cfg_storage.h / cfg_ota.h）
04_Common_Utils/          硬件无关工具库 + 自定义 .FLM
04_Debug_Tool/            日志、追踪、ITM/RTT、MPU 保护
```

---

## BSP 适配器模式

每个外设拆为五段，互不耦合，APP 只看到 vtable：

```
Bsp_Drivers/<device>/driver/             原始协议通信（禁止 OSAL）
Bsp_Drivers/<device>/handler/            handler 任务：读驱动 → 投队列
Platform_Interface/.../bsp_wrapper_*/    向 APP 暴露的抽象 vtable API
Platform_Interface/.../bsp_adapter_*/    将具体 driver 注册到 vtable
Bsp_Integration/<device>_integration/    组装 input_arg
```

新增同类外设 = 只替换 `driver` + `adapter`，APP 不动。

---

## 快速开始

```bash
cd 03_Firmware/01_APP

# === 改固件代码 ===
make                  # → build/helloworld.{elf,hex,bin}
make clean
make mem-report       # FLASH/RAM 占用图

# === 改 LVGL 资源图（不动 firmware） ===
make pack-assets      # → build/assets.bin（含 magic + 指针小图 + 240×240 表盘背景）
make flash-assets     # 经 SEGGER JFlash + 自定义 .FLM 直写 W25Q64 LVGL 分区
```

工具链：`arm-none-eabi-gcc`、GNU Make、Python（uv 管理工具脚本）、SEGGER JLink 系列（烧录 / Ozone 调试 / RTT 日志 / SystemView 追踪）。

LVGL 资源走外部 W25Q64 而非内部 Flash：指针小图由 firmware self-bootstrap 兜底（首次启动从 .rodata 写入），240×240 表盘背景太大不进 firmware，必须由 `make flash-assets` 经自定义 CMSIS .FLM 写入；运行时 `lv_port_extflash` 自定义 LVGL decoder 行级 streaming 渲染。详见 [固件架构与开发指南](03_Firmware/01_APP/README.md) 或 [01_APP/CLAUDE.md](03_Firmware/01_APP/CLAUDE.md)。

---

## 参考实现（当前硬件）

> 以下仅是当前验证用的参考实现，**架构本身不绑定**这些选型。

| 项目 | 当前选型 |
|---|---|
| MCU | STM32F411xE（Cortex-M4F） |
| RTOS | FreeRTOS v10.3.1 |
| 外设 | ST7789 LCD、CST816T 触摸、AHT21 温湿度、MPU6050 运动、WT588F02 语音、W25Q64 外部 Flash |

更换 MCU → 改写 `02_MCU_Platform` port 层与启动文件；更换 RTOS → 改写 `02_OS_Platform/OS_Implementation`；更换某个外设 → 替换该外设的 `driver` 与 `adapter`。

---

## CI/CD

GitHub Actions 自动构建（[c-cpp.yml](.github/workflows/c-cpp.yml)），Push/PR 到 master 触发，产出 elf/hex/bin/map，保留 30 天。

---

## 详细文档

- [固件架构与开发指南](03_Firmware/01_APP/README.md)
- [AI 辅助开发指引](03_Firmware/01_APP/CLAUDE.md)
