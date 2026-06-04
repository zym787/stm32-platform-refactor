# STM32F411 FreeRTOS 固件框架

基于 STM32F411xE + FreeRTOS 的嵌入式固件工程，采用严格分层 + BSP 适配器模式，支持多传感器并发读取、OSAL 可替换、双路调试输出（RTT + ITM/SWO）和 UART/Ymodem OTA 升级。

---

## 硬件平台

| 项目 | 规格 |
|---|---|
| MCU | STM32F411xE（Cortex-M4F，512KB FLASH，128KB SRAM） |
| 内部 Flash 布局 | Bootloader 32KB + OTA Flag 16KB + APP 464KB |
| RAM 布局 | APP RAM 121KB + RTT_RAM 7KB（`0x2001E400`，与 Bootloader 共址） |
| RTOS | FreeRTOS v10.3.1，heap_4，60KB 堆，1 kHz tick |
| FPU | 单精度硬浮点（`-mfpu=fpv4-sp-d16 -mfloat-abi=hard`） |
| 调试接口 | JLink（SWD / RTT / SWO / SystemView） |

---

## 工具链要求

| 工具 | 用途 |
|---|---|
| `arm-none-eabi-gcc` | 交叉编译 |
| `make` | 构建 |
| `uv` | 运行 `Tools/` 下的 Python 工具并管理依赖 |
| SEGGER JFlash | 烧录 `build/helloworld.hex` / 外部 W25Q64 资源 |
| SEGGER Ozone | 源码级调试，加载 `build/helloworld.elf` |
| SEGGER SystemView | RTOS 实时任务追踪 |
| JLink RTT Viewer | 日志输出，通道 0，波特率 1000000 |
| JLink SWO Viewer | ITM/SWO 输出（部分 tag 专用路径） |

---

## 构建

```bash
# === 改固件代码 ===
make                # 完整构建 -> build/helloworld.{elf,hex,bin,mxxx}
make clean          # 清理 build/
make mem-report     # 内存占用报告（Tools/mem_report.py）
make OPT=-O2        # 指定优化等级（默认 Makefile 当前为 -Og）

# === OTA 镜像（默认 all 已自动产 .mxxx；下面是单独触发） ===
make ota-image      # build/helloworld.bin -> build/helloworld.mxxx
                    # 16 B 头 + AES-256-CBC（Tools/ota_encrypt.py，uv 自动装 pycryptodome）

# === 改 LVGL 资源图（独立通道，不动 firmware） ===
make pack-assets    # -> build/assets.bin（magic + 指针小图 + 240x240 背景）
make flash-assets   # SEGGER JFlash CLI + 自定义 .FLM -> W25Q64 LVGL 分区
```

资源走外部 Flash 是为了让 240x240 表盘背景（169KB）不挤占内部 Flash。OTA 镜像（`.mxxx`）和固件 `.hex` 是同一份构建产物的两种封装：`.hex` 给 JFlash 首次量产烧录，`.mxxx` 给 Ymodem OTA 升级。

---

## 工程架构

当前源码采用 `00/01/02/03/04/05` 分层目录。`03_Platform/` 放稳定接口，`04_Impl/` 放具体实现。

```
00_Config/             项目级宏开关和地址/状态字唯一真源
01_App/                业务任务、初始化流程、ISR 派发、传感器验证
02_Service/            业务无关 service 抽象（OTA / storage 门面）
03_Platform/           OS / BSP / MCU / Middleware / Common 稳定接口
04_Impl/               OS / BSP / MCU / Middleware 具体实现
05_Common_Utils/       硬件无关工具与自定义 .FLM
05_Debug_Tool/         SEGGER SystemView、RTT 日志、ITM/SWO、MPU 保护
ST HAL / FreeRTOS      厂商中间件
ARM CMSIS / 硬件寄存器
```

依赖方向：`01_App` 和 `02_Service` 只能向下依赖 `03_Platform`、`04_Impl` 的公开头、`00_Config`、`05_Common_Utils`、`05_Debug_Tool`；底层不得反向 include `01_App/`。

### BSP 适配器模式

每个外设拆成接口和实现两侧：

```
03_Platform/platform_bsp/<category>/bsp_wrapper_<cat>/
    向 APP / Service 暴露抽象 vtable API

04_Impl/impl_bsp/Bsp_Drivers/<device>/driver/
    原始协议通信（寄存器 / SPI / I2C），禁止 OSAL 调用

04_Impl/impl_bsp/Bsp_Drivers/<device>/handler/
    Handler 任务：读驱动，投队列或通知 wrapper

04_Impl/impl_bsp/Adapter_Port/<category>/
    drv_adapter_<cat>_register()，把具体驱动挂到 wrapper vtable

04_Impl/impl_bsp/Bsp_Integration/<device>_integration/
    组装 input_arg（driver fn + OS 资源 + IO 描述符）
```

已实现设备：`aht21`（温湿度）、`mpu6050`（运动）、`em7028`（PPG 心率，流式 + lifecycle）、`wt588f02`（音频播报）、`st7789`（LCD 显示）、`cst816t`（触摸屏）、`w25q64`（外部 SPI NOR，承载 LVGL 资源 + OTA staging）。

### 任务管理

所有 FreeRTOS 任务集中登记在 `01_App/User_Task_Config/src/user_task_reso_config.c` 的 `g_user_task_cfg[]` 表中，包含任务名、栈大小、优先级、入口函数和参数。`01_App/User_Init/user_init.c` 在启动时遍历该表创建所有任务。

ISR 上下文禁止获取 IIC/SPI 总线互斥锁。ISR 只通过 `osal_notify` 唤醒 handler 任务，由线程上下文完成总线操作。

---

## 调试

### 日志系统（`DEBUG_OUT` 宏）

使用 `05_Debug_Tool/Debug_Log/inc/Debug.h` 中的 `DEBUG_OUT(level, tag, fmt, ...)` 输出日志。

两路输出路径同时有效，按 tag 分配：

- **RTT 路径**：大多数 tag -> EasyLogger -> RTT channel 0，RTT Viewer 按 Terminal Tab 分组显示。
- **ITM 路径**：`debug_is_itm_tag()` 中列出的 tag -> `printf()` -> ITM stimulus port 0 -> SWO Viewer。

RTT Terminal 分组：

| Terminal | 内容 |
|---|---|
| 0 | 默认（未显式路由的 tag） |
| 1 | AHT21 / 温湿度 |
| 2 | WT588 音频 |
| 3 | MPU6050 / 数据解析 |
| 4 | ST7789 TFT-LCD |
| 5 | CST816T 触摸 |
| 6 | W25Q64 SPI NOR |
| 7 | EM7028 PPG 心率 |
| 8 | 栈水位监控 |

### 调试流程

```
JFlash 烧录 hex
    ↓
Ozone 加载 elf（断点 / 变量监视）
    ↓
SystemView（RTOS 任务时序追踪）
    ↓
RTT Viewer / SWO Viewer（日志输出）
```

---

## 关键文件速查

| 文件 | 说明 |
|---|---|
| `Core/Inc/FreeRTOSConfig.h` | 堆大小、tick、优先级级别 |
| `STM32F411XX_FLASH.ld` | APP 链接脚本：RAM 121KB + RTT_RAM 7KB + APP Flash 464KB |
| `Core/Src/system_stm32f4xx.c` | APP vector table offset（`0x0000C000`） |
| `Core/Inc/main.h` | 引脚定义、全局 include |
| `03_Platform/platform_mcu/MCU_Core_IIC/inc/i2c_port.h` | I2C 类型切换（HW/SW）、互斥锁超时 |
| `00_Config/inc/cfg_storage.h` | W25Q64 LVGL/OTA 分区、资源 offset/size、magic |
| `00_Config/inc/cfg_ota.h` | OTA flag 结构、状态字、内部 flag 地址 |
| `05_Debug_Tool/Debug_Log/inc/Debug.h` | 日志 tag 定义、过滤、RTT/ITM 路由 |
| `Makefile` | 源文件列表、编译选项；含 `pack-assets` / `flash-assets` target |

---

## 外部 Flash LVGL 资源（W25Q64）

LVGL 指针小图 + 240x240 表盘背景全部托管在外部 W25Q64 SPI NOR 上，省下内部 Flash 容纳更多业务代码。固件改动和资源更新走两条独立通道：

```
改固件代码：             改 LVGL 资源图：
  make                     make flash-assets
  ↓                        ↓
  hex                      assets.bin
  ↓                        ↓
  JFlash 内部 Flash         JFlash + 自定义 .FLM -> W25Q64
                            （magic + 指针小图 + 240x240 背景）
```

### 三套地址空间

| 视角 | LVGL 起点 | 大小 | 谁用 |
|---|---|---|---|
| LVGL local（软件） | `0x000000` | 3 MB | `Read_LvglData(addr,...)` |
| W25Q64 物理 | `0x300000` | 3 MB | SPI2 驱动直接寻址 |
| JLink 虚拟（FLM） | `0x90000000` | 3 MB | JFlash / Ozone 工程 |

### Bootstrap 不对称设计

| 资源 | 大小 | firmware .rodata 备份 | 自举行为 |
|---|---|---|---|
| 指针小图（fen / miao / time） | ~2.7KB / 张 | 有 | magic 不匹配自动从 .rodata 写回 |
| 240x240 表盘背景（biaopan1） | 169KB | 无 | 仅 `make flash-assets` 写入；未写时背景全白 |

首次烧录新机器：先 `make` + JFlash hex，再执行 `make flash-assets` 一次写入背景，之后每次开机 firmware 直接从 W25Q64 streaming 读取。

### 渲染路径

```
LVGL render -> lv_port_extflash decoder
              ↓ (info_cb 用 magic 识别这张归本 decoder)
              ↓ (open_cb 设 img_data=NULL，切到行级模式)
              ↓ (read_line_cb 调 Read_LvglData 抓 720B/行)
              service_storage/storage_manager_task
              ↓ (event_group + binary sema 把 BSP async 包成阻塞)
              externflash 异步驱动
              ↓ (SPI2 PB10/14/15 + CS PB13)
              W25Q64 SPI NOR
```

每行约 1ms，全屏 240 行约 240ms（首屏可见小延迟）；运行时表盘只重绘指针覆盖的脏区，性能可接受。

### 自定义 JLink .FLM

`05_Common_Utils/01_Flash_Algorithm/W25Q64_8M_FLM.FLM` 是本板适配版 Flash 算法，源码工程在 `std_program_algorithms/`。`Devices.xml` 注册自定义设备 `STM32F411CE_W25Q64`，把 W25Q64 SPI bank 挂在 JLink 虚拟地址 `0x90000000`。FLM 内部把 JLink 地址重映射到 W25Q64 物理 `0x300000`（LVGL 分区起点），因此 JFlash 工具链只能写 LVGL 分区，无法误伤 OTA / FlashDB / FATFS。

---

## OTA 升级链路

PC -> UART1 -> APP（Ymodem 收 + 写 W25Q64）-> soft reset -> Bootloader（AES 解密 + 拷贝到内部 APP 槽 + 回滚兜底）。触发不走 shell，靠 UART 魔术字：

```
PC 端                       APP（运行中）                     Bootloader（reset 后）
─────                       ────────────                     ──────────────────
0x11 22 33  ─UART1 TX─►  listener 扫到 -> 设 OTA_START
            Ymodem 1K包  ymodem_recv_task 串行 Ymodem_Receive
                         firmware_upgrade_task 4KB 缓冲 ->
                         Write_OtaData(W25Q64 OTA 分区)
0x77 88 99  ─UART1 TX─►  listener 扫到 -> NVIC_SystemReset()
                                                            读 ota_flag@0x08008000
                                                            state=0x22 -> 解密
                                                            BLOCK_1 -> BLOCK_2 ->
                                                            内部 Flash + state=0x33
                                                                                       │
                                                            jump_to_app ◄──────────────┘
new APP 启动 ◄────────  user_init 检 state=0x33/0x44 ->
                       auto-confirm 写 state=0x00 ->
                       iwdg_feeder 持续喂狗
```

### service 抽象层

OTA 不属于业务任务，也不属于 BSP 驱动，放在 `02_Service/service_ota/`；W25Q64 阻塞门面放在 `02_Service/service_storage/`：

```
02_Service/service_ota/
├── inc/
│   ├── ota_transport.h        OTA 传输抽象（不绑定具体 UART id）
│   ├── ota_storage.h          OTA 存储抽象
│   ├── upgrade_service.h      ota_flag_read/write
│   └── firmware_upgrade.h     服务入口 / signal_start / signal_apply / flush_staged
├── src/
│   ├── upgrade_service.c       内部 Flash 0x08008000 读写
│   ├── iwdg_feeder_task.c      F411 IWDG 起后不可关，always-on 500ms 喂
│   ├── ota_uart_listener.c     UART1 RX + 3-byte 滑窗扫魔术字
│   └── firmware_upgrade_task.c 4KB sector 缓冲 + Write_OtaData
└── adapters/
    ├── uart1_ota_transport.c   ota_transport_* -> mcu_uart_*
    └── w25q64_ota_storage.c    ota_storage_* -> service_storage

02_Service/service_storage/
├── inc/service_storage_facade.h
└── src/storage_manager_task.c   Read/Write_LvglData 与 Read/Write_OtaData
```

后续业务无关 service（FOTA、配网、电池策略等）都进 `02_Service/`。

### 加密格式 + 工具

`make ota-image` 调 `Tools/ota_encrypt.py`（uv 自动装 pycryptodome），按 bootloader 的解密期望格式封装：

```
[12 B 零 | 4 B LE app_size | helloworld.bin | 0xFF pad 到 16 B 对齐]
        ↓
AES-256-CBC 加密（key/iv = bootmanager.c 硬编码 32x{0x31,0x32} 交替）
        ↓
build/helloworld.mxxx
```

硬编码 key 仅过渡用，上线前必须换成运行时注入。

### 关键地址 & 状态字（`00_Config/inc/cfg_ota.h`）

| 项 | 值 | 谁写 | 谁读 |
|---|---|---|---|
| `CFG_OTA_FLAG_ADDRESS` | `0x08008000`（内部 Flash Sector 2，16KB） | APP / Bootloader | 同 |
| `CFG_OTA_FLAG_MAGIC` | `0xA55A5AA5` | APP / Bootloader | 同 |
| W25Q64 OTA staging | `0x000000`（1MB 分区，与 `MEMORY_OTA_START_ADDRESS` 一致） | APP firmware_upgrade_task | Bootloader |
| W25Q64 BLOCK_2 staging | `0x080000`（解密后中转） | Bootloader 解密路径 | 同 |
| 内部 APP 槽 | `0x0800C000`（464KB，sectors 3-7） | Bootloader | APP 启动 |
| 当前 APP 大小 | linker 符号 `__app_size__` | APP user_init 写 ota_flag.current_app_size | Bootloader 回滚 backup 用 |

### 状态机（沿用上游 `EE_OTA_*` 数值）

| state | 谁写 | 含义 / 触发 |
|---|---|---|
| `0xFF INIT_NO_APP` | magic 失配兜底 | 全片 erase 后首次启动 |
| `0x00 NO_APP_UPDATE` | APP user_init / Bootloader CHECKING 路径 | APP 已确认健康，正常跳 APP |
| `0x22 DOWNLOAD_FINISHED` | APP firmware_upgrade_task | Ymodem 收完 + W25Q64 写完 + ota_flag 已更新 |
| `0x33 APP_CHECK_START` | Bootloader ota_apply_update | 解密 + 拷贝完成，跳 APP 等 confirm |
| `0x44 APP_CHECKING` | Bootloader CHECK_START 分支 | 跳 APP 前再推进一次，防 reset 中断后死循环 |

APP `user_init` 看到 `0x33` 或 `0x44` 都 auto-confirm；若 IWDG 在约 6s 内没被 APP 喂上（说明新镜像跑不起来），Bootloader CHECKING 分支兜底回滚。

### 一次完整使用流程

```bash
cd 03_Firmware/01_APP
make                                  # build/helloworld.{hex,mxxx}
# 首次量产：
#   JFlash 烧 01_APP/build/helloworld.hex      到 0x0800C000
#   JFlash 烧 00_Bootloader/build/bootloader.hex 到 0x08000000
# 之后升级：
#   PC 端 UART1 工具发 3 字节 "11 22 33" -> 立刻 Ymodem 发 build/helloworld.mxxx
#   收完后再发 3 字节 "77 88 99" -> MCU reset -> bootloader 完成升级
```
