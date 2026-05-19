# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 构建命令

```bash
make                # 完整构建 → build/helloworld.{elf,hex,bin,mxxx}
                    # .mxxx 是 OTA 加密镜像，由 build-core 自动调
                    # Tools/ota_encrypt.py (uv run) 产出
make clean          # 删除 build/ 目录
make mem-report     # 内存占用报告（Tools/mem_report.py，含 link region 占用图）
make OPT=-O2        # 覆盖优化等级（默认 -Os；DEBUG=1 时切到 -Os 才能放下 .rodata）
make ota-image      # 单独触发 OTA 镜像生成（默认 all 已包含）

make pack-assets    # 仅打包 LVGL 资源 → build/assets.bin（不动 firmware）
make flash-assets   # pack + JFlash CLI 烧 W25Q64 LVGL 分区（详见 "外部 Flash 资源" 节）
```

目标芯片：STM32F411xE（Cortex-M4F），工具链：`arm-none-eabi-gcc`，默认编译选项：`-Os -g -gdwarf-2`。

无正式测试框架。应用级验证任务位于 `01_APP/User_Sensor/`，在目标硬件上运行（如 `temp_humi_test_task_a/b` 用于并发读取验证）。

## 架构总览

严格的分层依赖——上层只依赖下层：

```
01_APP                  ← 业务逻辑、任务定义、传感器测试
02_*_Platform           ← 四个能力层（见下文）
ST HAL / FreeRTOS       ← 厂商中间件
ARM CMSIS / 硬件        ← 寄存器级
```

### 平台层（`02_*/`）

| 目录 | 职责 |
|---|---|
| `02_OS_Platform/` | 对 FreeRTOS 的 OSAL 封装。`OS_Wrapper/inc/` 是公开 API；`OS_Implementation/` 包含 FreeRTOS 映射实现。替换 `OS_Implementation` 即可切换 RTOS。 |
| `02_BSP_Platform/` | 使用适配器模式的传感器/外设驱动（详见下文）。 |
| `02_MCU_Platform/` | 芯片级 port 层：`MCU_Core_IIC/SPI/GPIO/Systick/DWT` 总线与时钟、`MCU_Core_Watchdog/`（`mcu_watchdog_refresh()`）、`MCU_Core_IFlash/`（`mcu_iflash_erase_sector + program_words`，函数体内 `__disable_irq()` 包裹整段防 F411 单 bank flash 取指死锁）。所有寄存器级触碰都收在此层，service / APP 不直接调 HAL。同时支持硬件 I2C（HAL）和软件 I2C 位操作（SCL=PB14，SDA=PB15）。 |
| `02_Middleware_Platform/` | EasyLogger（异步日志），LetterShell（嵌入式 CLI），LVGL v8.3（GUI 库）。 |

### BSP 适配器模式（`02_BSP_Platform/`）

每个外设遵循以下三层结构：

```
Bsp_Drivers/<device>/driver/    ← 原始寄存器/协议通信，禁止调用 OSAL
Bsp_Drivers/<device>/handler/   ← Handler 线程逻辑（读取驱动，投递到队列）
Platform_Interface/<category>/
  bsp_wrapper_<cat>/            ← 向 01_APP 暴露的抽象 vtable API
  bsp_adapter_port_<cat>/       ← 将具体驱动注册到 wrapper vtable
Bsp_Integration/<device>_integration/  ← 将驱动+OS 资源组装为 input_arg 结构体
```

`bsp_wrapper_*` 头文件定义公开 API 及 vtable 结构体。`bsp_adapter_port_*` 头文件仅暴露 `drv_adapter_<cat>_register()`。集成层负责组装传递给 handler 线程的 `*_input_arg` 结构体。

Wrapper API 风格按设备类型选择：

| 风格 | 适用 | 形态 |
|---|---|---|
| 请求-响应（sync/async） | 单次按需读取的传感器，如 `temp_humi` | `*_read_*_sync(life_time)` 阻塞，`*_read_*_async(cb, life_time)` 回调 |
| 流式（streaming） | 持续产帧的传感器，如 `motion`、`heart_rate` | `*_drv_get_req(timeout) → *_get_data_addr() → *_read_data_done()`；`heart_rate` 额外提供 `start/stop/reconfigure` lifecycle |

现有设备：`aht21`（温湿度）、`mpu6050`（运动）、`wt588f02`（音频）、`st7789`（LCD 显示）、`cst816t`（触摸屏）、`w25q64`（外部 SPI NOR，五段已完成，承载 LVGL 资源分区）、`em7028`（PPG 心率，category=`heart_rate`，流式 + lifecycle，frame 类型 `wp_ppg_frame_t`）。

### 应用层（`01_APP/`）

- **任务表**：`User_Task_Config/src/user_task_reso_config.c` — 定义 `g_user_task_cfg[]`，包含所有应用任务的名称、栈大小、优先级、入口函数和参数。
- **任务优先级**（定义于 `user_task_reso_config.h`）：`PRI_EMERGENCY`、`PRI_HARD_REALTIME`、`PRI_SOFT_REALTIME`、`PRI_NORMAL`、`PRI_BACKGROUND`。
- **任务创建**：`User_Init/user_init.c` 遍历 `g_user_task_cfg[]`，逐条调用 `osal_task_create()`；失败时回滚。
- **IO 注册**：`User_Init/Platform_IO_Register/` — 启动时将硬件 IO 绑定到驱动适配器。
- **ISR 派发**：`User_Isr_handlers/` — ISR 通过 OSAL notify 唤醒任务，避免在中断上下文中阻塞（防止 IIC 互斥锁死锁）。
- **栈水位监控**：`User_Task_Config/src/task_higher_water_monitor.c` — 运行时任务栈占用监控。

### 服务层（`01_SERVICE/`）

业务无关 service 抽象，与 `01_APP/` 平级。Service 只能向下调 `02_*_Platform` / `04_Common_Utils` / `03_Config`，**禁止反向依赖 `01_APP/` 任何代码**。

| 子模块 | 职责 |
|---|---|
| `service_storage/` | 异步 BSP externflash 上的阻塞门面：`Read_/Write_LvglData`、`Read_/Write_OtaData` + `storage_manager_task`。APP（LVGL 资源）与 `service_ota`（Ymodem staging）共享同一条单消费者队列。 |
| `service_ota/` | OTA 升级链路（详见 "OTA 升级链路" 节）。`ota_flag_read/write` 经 `MCU_Core_IFlash` 写内部 Flash；`iwdg_feeder_task` 经 `MCU_Core_Watchdog` 喂狗。 |

后续 FOTA、配网、电池策略等 service 都进这一层。

### OSAL 层（`02_OS_Platform/`）

`OSAL_Common/inc/osal_common_types.h` 定义项目全局共用类型：`osal_task_handle_t`、`osal_queue_handle_t`、`osal_mutex_handle_t`、`osal_tick_type_t` 等。始终通过 `osal_wrapper_adapter.h` 包含。

### 通用工具层（`04_Common_Utils/`）

与硬件/业务无关的工具代码（StrUtils、CRC16、MemPool、ByteConverter 等），所有层均可复用，不依赖 OSAL 或 BSP。

### 配置层（`03_Config/`）

用于项目级宏开关（`CFG_` 前缀）：功能特性开关、板级 IO 映射、RTOS 资源大小。当前包含 `cfg_storage.h`（W25Q64 LVGL 子分区 magic + 资源 offset/size）和 `cfg_ota.h`（OTA flag 结构 + magic + 状态宏，与 bootloader `Tasks/Bootmanager/inc/ota_flag.h` 必须保持字节兼容）。

### 调试工具（`04_Debug_Tool/`）

#### 日志系统（`Debug.h`，`DEBUG_OUT` 宏）

两路输出路径同时有效，按 tag 选择路由：

- **RTT 路径**（默认）：经由 EasyLogger → `SEGGER_RTT_SetTerminal()` → RTT 物理通道 0，在 J-Link RTT Viewer 中按 Terminal Tab 分组显示。
- **ITM/SWO 路径**：`debug_is_itm_tag()` 中列出的 tag 绕过 EasyLogger，通过 `printf()` → `__io_putchar()` → ITM stimulus port 0 输出，在 JLink SWO Viewer 或 Ozone SWO 终端中可见。

RTT Terminal 分组（`DEBUG_RTT_CH_*`）：

| Terminal | 常量 | 覆盖的 tag |
|---|---|---|
| 0 | `DEBUG_RTT_CH_DEFAULT` | 所有未显式路由的 tag |
| 1 | `DEBUG_RTT_CH_SENSOR0` | AHT21 / 温湿度相关 |
| 2 | `DEBUG_RTT_CH_SENSOR1` | WT588 handler / 测试 |
| 3 | `DEBUG_RTT_CH_SENSOR2` | MPU6050 / 数据解析 |
| 4 | `DEBUG_RTT_CH_DISPLAY` | ST7789 TFT-LCD |
| 5 | `DEBUG_RTT_CH_TOUCH`   | CST816T 触摸 |
| 6 | `DEBUG_RTT_CH_STORAGE` | W25Q64 SPI NOR Flash |
| 7 | `DEBUG_RTT_CH_PPG`     | EM7028 PPG 心率 |
| 8 | `DEBUG_RTT_CH_STACK`   | 栈水位监控 |

新增 tag 步骤：
1. 在 `Debug.h` 中定义 `*_LOG_TAG` 字符串常量。
2. 将其加入 `debug_is_tag_allowed()`（启用输出）。
3. 在 `debug_tag_to_rtt_channel()` 中指定 Terminal，或加入 `debug_is_itm_tag()` 走 ITM 路径。

新增 ITM-only tag 步骤：
1. 在 `Debug.h` 中定义 `*_ITM_LOG_TAG` 常量。
2. 将其加入 `debug_is_itm_tag()`。无需修改 `elog_port.c` 或 RTT 配置。

#### SEGGER SystemView

实时 OS 追踪，通过 RTT 传输。8KB SRAM 专用于 RTT buffer（链接脚本中 `RTT_RAM` 区域，地址 `0x2001E000`）。

## 调试工作流

| 操作 | 工具 |
|---|---|
| 烧录固件 | SEGGER JFlash — 打开 `build/helloworld.hex`，目标 STM32F411xE |
| 源码调试 | SEGGER Ozone — 加载 `build/helloworld.elf`，通过 JLink 连接 |
| OS 任务追踪 | SEGGER SystemView — 通过 JLink 附加到运行中的目标 |
| printf 日志 | JLink RTT Viewer — 通道 0，1000000 波特率 |
| SWO 输出 | JLink SWO Viewer 或 Ozone SWO 终端，波特率由 `itm_trace_init(cpu_hz, swo_hz)` 配置 |

典型流程：JFlash 烧录 → Ozone 断点/监视 → SystemView RTOS 时序 → RTT Viewer 日志输出。

## 硬件

- **MCU**：STM32F411xE — Cortex-M4F，512KB FLASH，128KB SRAM
- **RTOS**：FreeRTOS v10.3.1，heap_4，60KB 堆，1 kHz tick，CMSIS-RTOS V2 API 可用
- **FPU**：单精度硬浮点（`-mfpu=fpv4-sp-d16 -mfloat-abi=hard`）
- **链接脚本**：`STM32F411XX_FLASH.ld` — 120KB 用户 RAM + 8KB RTT RAM

### 关键引脚分配（`Core/Inc/main.h`）

| 信号 | 引脚 |
|---|---|
| 软件 I2C SCL | PB14 |
| 软件 I2C SDA | PB15 |
| SPI1 CS/RST/DC | PA3/PA4/PA6 |
| 软件 SPI SCK/MISO/MOSI | PA5/PA6/PA7 |
| **SPI2 SCK / MISO / MOSI（W25Q64）** | **PB10 / PB14 / PB15** |
| **SPI2 CS（W25Q64）** | **PB13** |
| WT588 busy | PA12 |
| 触摸屏中断 TINT | PB2（EXTI2） |

## 外部 Flash LVGL 资源（W25Q64）

LVGL 指针小图 + 240×240 表盘背景**全部托管在 W25Q64 上**，省下内部 Flash 容纳 EM7028 等业务代码。资源走两条独立路径，互不干涉：改 firmware 走 `make`，改图走 `make flash-assets`。

### 三套地址空间（关键）

| 视角 | LVGL 起点 | 大小 | 谁用 |
|---|---|---|---|
| LVGL local（软件层） | `0x000000` | 3 MB | `Read_LvglData(addr,...)` 接口 |
| W25Q64 物理 | `0x300000` | 3 MB | SPI2 驱动直接寻址 |
| JLink 虚拟（FLM） | `0x90000000` | 3 MB | JFlash / Ozone 工程 |

`storage_manager_task` 做 `LVGL → W25Q64`：`addr + MEMORY_LVGL_START_ADDRESS (0x300000)`。  
`W25Q64_8M_FLM.FLM` 做 `JLink → W25Q64`：`adr - 0x90000000 + 0x300000`。**FLM 范围被锁在 LVGL 分区内**，工具链根本无法触碰 OTA / FlashDB / FATFS / Reserved。

### 分区布局（`03_Config/inc/cfg_storage.h`）

```
W25Q64 物理        LVGL local      内容
0x300000           0x000000        magic 0xA55A5AA5 (4 B)
0x300100           0x000100        fen_70x5    (1050 B)   ← .rodata 备份
0x300600           0x000600        miao_70x5   (1050 B)   ← .rodata 备份
0x300B00           0x000B00        time_40x5   ( 600 B)   ← .rodata 备份
0x302000           0x002000        biaopan1    (172800 B) ← W25Q64 唯一来源
```

### 软件链路

```
01_APP/User_Sensor/storage/
├── storage_manager_task.c       ← BSP async API 包成阻塞 Read/Write_LvglData
├── storage_assets.c             ← _ext lv_img_dsc_t 描述符 + bootstrap
└── inc/user_externflash_manage.h

02_Middleware_Platform/lv-gl/lvgl_port/
└── lv_port_extflash.c           ← 自定义 LVGL decoder，行级 streaming biaopan1
```

**Bootstrap 策略不对称**：3 张 sprite 在 firmware `.rodata` 里有 seed，magic 不匹配时自动重写 W25Q64；biaopan1 太大（169 KB），**仅由 `make flash-assets` 写入**。如果 W25Q64 全空且没跑 flash-assets，启动后 sprite 仍能恢复，但表盘背景会是全白（0xFF）。首次烧机器：`make flash-assets` 一次即可。

### LVGL 自定义 decoder（`lv_port_extflash`）

biaopan1 的 `lv_img_dsc_t.data` 不指像素，指 `lv_extflash_meta_t`（含 magic + offset + 几何）。decoder 的 `info_cb` 用 magic 识别"这张归我管"，`open_cb` 设 `img_data=NULL` 让 LVGL 切到行级模式，`read_line_cb` 调 `Read_LvglData` 抓一行 720 B 给 LVGL。每行 ~1 ms，全屏 240 行 ~240 ms（首屏可见的渲染延迟），但表盘运行时只重绘针覆盖的脏区，可接受。

### 烧录工具链

| 命令 | 作用 |
|---|---|
| `make` | 编固件（不包 biaopan，省 ~169 KB Flash） |
| `make pack-assets` | `Tools/pack_assets.py` 解析 cfg_storage.h + lv_conf.h + LVGL .c 数组 → `build/assets.bin`（4KB-aligned） |
| `make flash-assets` | pack + `JFlash.exe -openprj ... -auto -exit` 经 .FLM 直写 W25Q64 LVGL 分区 |

JLink 设备 `STM32F411CE_W25Q64` 注册在 `%APPDATA%\SEGGER\JLinkDevices\ST\STM32F4\Devices.xml`。FLM 是本板适配版（SPI2/PB10/14/15、CS PB13），源码在 `std_program_algorithms/`（Keil 工程）。

## OTA 升级链路

PC → UART1 → APP（Ymodem 收 + 写 W25Q64 OTA 分区）→ NVIC_SystemReset → Bootloader（AES-256-CBC 解密 + BLOCK_1→BLOCK_2→内部 Flash + 回滚兜底）。**触发不走 shell**，靠 UART 魔术字 `0x11 22 33`（启动）/ `0x77 88 99`（应用）。

### 分层（v4 — MCU port 接管 HAL，adapter 进 SERVICE）

```
02_MCU_Platform/MCU_Core_UART/          ← UART 在 MCU 端口（与 IIC/SPI/IFlash 同辈）
  uart_port/inc/mcu_uart_port.h         稳定 API（uart_id 参数化）
  uart_port/src/mcu_uart_port.c         STM32 HAL + ISR dispatch
                                        + USART1_IRQHandler 也在这

01_SERVICE/service_ota/                 ← OTA 业务 + 板级适配同位
  inc/ota_transport.h                   抽象（不绑定具体 UART id）
  inc/ota_storage.h                     抽象
  inc/firmware_upgrade.h                服务入口 / 任务声明
  inc/upgrade_service.h                 ota_flag 持久化
  src/ota_uart_listener.c               状态机
  src/firmware_upgrade_task.c           consumer
  src/iwdg_feeder_task.c                喂狗
  src/upgrade_service.c                 ota_flag 读写（走 MCU_Core_IFlash）
  adapters/                             ← 板级 wiring
    uart1_ota_transport.c               1-行翻译：ota_transport_* → mcu_uart_*
                                        (OTA_UART_ID == MCU_UART_1)
                                        + firmware_upgrade_signal_apply
                                          (NVIC_SystemReset，待 MCU_Core_Reset)
    w25q64_ota_storage.c                ota_storage_* → service_storage Write/Read_OtaData

02_Middleware_Platform/Ymodem/          ← 中间件，0 HAL include
  src/ymodem.c                          调 ota_transport_* 走全链路
```

调用栈：

```
ota_service_task                    ┐
  ota_transport_listen_byte_wait    │  service 层（业务）
    ↓                               ┘
  mcu_uart_recv_byte_wait(MCU_UART_1, ...)   ← adapter 1 行翻译
    ↓                               ┐
  osal_sema_take(s_state[1].byte_sem)         MCU port 内部
    ↓                                         （HAL + ISR + OS）
  [ ISR fires when byte arrives ]
    HAL_UART_RxCpltCallback
      state_for_handle(huart) → &s_state[1]
      osal_sema_give_from_isr(byte_sem)     ┘
```

替换 transport：
- 换 UART：改 `adapters/uart1_ota_transport.c` 里 `OTA_UART_ID` 一行
- 换 BLE：新增 `adapters/bleN_ota_transport.c` 实现 `ota_transport.h`，service / Ymodem / MCU_Core_UART 都不动
- 换 MCU：替换 `mcu_uart_port.c` 一个文件即可（API 不变），service / adapter / Ymodem 都不动

### 链路任务

| 任务 | 优先级 | 职责 |
|---|---|---|
| `ota_service_task` | `PRI_SOFT_REALTIME` | 状态机 `SCAN_START → YMODEM_ACTIVE → SCAN_APPLY`：`ota_transport_listen_byte_*` 1-byte 滑窗扫 `0x11 22 33` magic → 调 `Ymodem_Receive()`（内部走 `ota_transport_frame_*`）→ `firmware_upgrade_flush_staged()` + `ota_flag_write()` → IT 模式扫 `0x77 88 99` apply magic → `firmware_upgrade_signal_apply()`（adapter 内 `NVIC_SystemReset`） |
| `firmware_upgrade_task` | `PRI_SOFT_REALTIME` | consume `Queue_AppDataBuffer`，4 KB sector 缓冲后调 `ota_storage_write` 写下层（默认 W25Q64） |
| `iwdg_feeder_task` | `PRI_BACKGROUND` | always-on 500 ms 喂狗（F411 IWDG 起后不可关）|

OS 全局对象按所有权分两组：

- **adapter 内部**（`uart1_ota_transport.c` 私有）：
  - `s_byte_sem`（binary）—— `HAL_UART_RxCpltCallback` 给一次
  - `s_frame_queue`（uint16_t × 4）—— `HAL_UARTEx_RxEventCallback` 给段长
  - 创建/管理由 `ota_transport_init()` 处理
- **service 共享**（`firmware_upgrade_task.c` 定义，Ymodem extern）：
  - `Queue_AppDataBuffer`（`Ymodem_RxContext_t*` × 2）—— Ymodem user_handler → consumer
  - `Semaphore_ExtFlashState`（binary）—— consumer 写完一包 → Ymodem 切 ping-pong buffer
  - 创建由 `firmware_upgrade_service_init()` 处理（一站式：post-OTA verify + transport_init + storage_init + service resources）

### UART1 RX 双路（互斥不并存）

| 模式 | 谁用 | 抽象 API |
|---|---|---|
| 中断单字节 | `ota_service_task` 在 SCAN_START / SCAN_APPLY 状态 | `ota_transport_listen_byte_arm` + `_wait` |
| DMA-idle frame | `Ymodem_Receive` 整段（YMODEM_ACTIVE 状态内） | `ota_transport_frame_arm` + `_is_armed` + `_wait` + `_stop` |

具体 HAL 实现细节封在 `uart1_ota_transport.c`：IT single-shot → magic 第 3 字节命中自然消耗；YMODEM_ACTIVE 进入时 RxState 已经回 READY，frame_arm 直接成功。session 结束（无论成败）后 `ota_service_task` 调 `listen_byte_arm` 重 arm IT 回到扫 magic。任一时刻只有一个 HAL 回调被武装。

### 加密格式（`Tools/ota_encrypt.py`）

`make ota-image` 跑 Python 脚本（pycryptodome，uv 自动装）：

```
[12 B 零 | 4 B LE app_size | helloworld.bin | 0xFF pad 到 16 B 对齐]
        ↓
AES-256-CBC 加密（key/iv = bootmanager.c 硬编码 32×{0x31,0x32} 交替，硬编码仅过渡）
        ↓
build/helloworld.mxxx
```

bootloader 的 `exA_to_exB_AES` 解密首块取 bytes[12..15] 当 LE uint32 = app_size。

### 关键地址与状态字（`03_Config/inc/cfg_ota.h`）

| 项 | 值 | 说明 |
|---|---|---|
| `CFG_OTA_FLAG_ADDRESS` | `0x08008000` | 内部 Flash Sector 2（16 KB），存 `ota_flag_t` |
| `CFG_OTA_FLAG_MAGIC` | `0xA55A5AA5` | magic，与空 sector（0xFFFFFFFF）区分 |
| W25Q64 OTA staging | `0x000000` | 1 MB，`MEMORY_OTA_START_ADDRESS` |
| 内部 APP 槽 | `0x0800C000` | 464 KB（sectors 3-7）|
| linker `__app_size__` | `LOADADDR(.data) + SIZEOF(.data) - ORIGIN(FLASH)` | 当前 APP 字节数，写入 `ota_flag.current_app_size` 供 bootloader 回滚备份 |

### 状态机（`CFG_OTA_*` 数值）

| state | 谁写 | 触发 |
|---|---|---|
| `0xFF INIT_NO_APP` | （magic 失配兜底） | 全片 erase 首启 |
| `0x00 NO_APP_UPDATE` | APP user_init / Bootloader CHECKING 兜底 | APP 已 confirm，正常跳 APP |
| `0x22 DOWNLOAD_FINISHED` | APP `ymodem_recv_task` | Ymodem 完成 + W25Q64 写完 |
| `0x33 APP_CHECK_START` | Bootloader `ota_apply_update` 末尾 | 解密 + 拷贝完成，跳 APP 等 confirm |
| `0x44 APP_CHECKING` | Bootloader `OTA_StateManager::CHECK_START` 分支 | 跳 APP 前再推进一次（防 reset 中断后死循环）|

APP `user_init` 看到 `0x33` 或 `0x44` 都 auto-confirm 写 `0x00`；若 IWDG 在 6 s 内没被 APP 喂上，bootloader CHECKING 分支兜底回滚。

### 易踩坑

- **W25Q64 Page Program 跨 page 回卷**：`bsp_w25q64_driver.c::w25q64_write_data_erase` 必须按 256 B page 边界拆 chunk，erase 仍 per-sector (4 KB)。否则单个 Page Program 跨 page 时 W25Q64 地址回卷到 page 起点 → 后续字节覆盖前面字节
- **APP `Write_OtaData` 每次 erase 整 sector**：sub-sector 写入会抹掉同 sector 之前的数据。`firmware_upgrade_task.c` 用 4 KB `s_sector_buf[]` 攒满再写，ymodem_recv_task 调 `firmware_upgrade_flush_staged()` 冲尾段
- **内部 Flash 操作必须关中断**：F411 单 bank flash erase 阻塞 ~1 s，期间 ISR 在 flash 取指会死锁。`MCU_Core_IFlash/iflash_port.c` 已经在 `mcu_iflash_erase_sector` / `mcu_iflash_program_words` 函数体内用 `__disable_irq()` + 出口恢复 PRIMASK 兜住，service 层调用方不用再手动关
- **UART1 polled HAL 会饿死 PRI_BACKGROUND**：listener 必须走中断/DMA 模式，否则 `iwdg_feeder` 抢不到 CPU → IWDG fire 整机 reset

## 新增外设驱动步骤

遵循现有 BSP 适配器模式：
1. `02_BSP_Platform/Bsp_Drivers/<device>/driver/` — 原始设备通信（禁止 OSAL）
2. `02_BSP_Platform/Bsp_Drivers/<device>/handler/` — 读取驱动的 handler 线程
3. `02_BSP_Platform/Platform_Interface/<category>/bsp_wrapper_<cat>/` — 抽象 vtable API
4. `02_BSP_Platform/Platform_Interface/<category>/bsp_adapter_port_<cat>/` — 将驱动注册到 vtable
5. `02_BSP_Platform/Bsp_Integration/<device>_integration/` — 组装 `*_input_arg` 结构体
6. 在 `01_APP/User_Init/Platform_IO_Register/` 中注册硬件 IO
7. 在 `User_Task_Config/src/user_task_reso_config.c` 的 `g_user_task_cfg[]` 中添加任务项
8. 将所有新增 `.c` 文件加入 `Makefile` 的 `C_SOURCES`

**ISR 规则**：禁止在中断上下文中获取 IIC 总线互斥锁。使用 `osal_notify` 唤醒 handler 任务，由线程上下文获取互斥锁。

## 关键配置文件

| 文件 | 控制内容 |
|---|---|
| `Core/Inc/FreeRTOSConfig.h` | 堆大小、tick 频率、优先级级别、启用特性 |
| `Core/Inc/stm32f4xx_hal_conf.h` | 编译哪些 ST HAL 模块 |
| `STM32F411XX_FLASH.ld` | 内存映射、段放置 |
| `Core/Inc/main.h` | 引脚定义、全局包含 |
| `02_MCU_Platform/MCU_Core_IIC/i2c_port/inc/i2c_port.h` | I2C 总线类型（硬件/软件）、互斥锁超时 |
