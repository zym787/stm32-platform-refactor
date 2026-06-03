# impl_bsp — 外设驱动实现

按"设备类别"对外暴露 vtable，让 APP 不感知具体型号。同类设备替换 = 换 `driver` + `adapter`，APP / Service 不动。公开 wrapper 位于 `03_Platform/platform_bsp/`，具体驱动和 adapter 位于本目录。

详细五段式 + 新增外设步骤见 [../../CLAUDE.md](../../CLAUDE.md) "BSP 适配器模式"节。

## 目录结构

```
04_Impl/impl_bsp/
├── Bsp_Drivers/<device>/
│     ├── driver/      ← 裸协议通信（寄存器 / SPI / I2C）。禁止 OSAL 调用
│     └── handler/     ← Handler 任务：读驱动 → 投队列 / 通知 wrapper
├── Bsp_Integration/<device>_integration/
│                       组装 `*_input_arg`（driver fn + OS 资源 + IO 描述符）
└── Adapter_Port/<category>/
      └── `drv_adapter_<cat>_register()` 把具体驱动挂到 vtable

03_Platform/platform_bsp/<category>/
└── bsp_wrapper_<cat>/              向 01_App / 02_Service 暴露的抽象 vtable API
```

## 当前实现

| 设备 | 类别（platform_bsp） | wrapper API 风格 |
|---|---|---|
| `aht21` | `temp_humi/` | sync/async 单次读 |
| `mpu6050` | `motion/` | 流式 `get_req → get_data_addr → read_data_done` |
| `em7028` | `heart_rate/` | 流式 + lifecycle（`start/stop/reconfigure`），frame=`wp_ppg_frame_t` |
| `st7789` | `display/` | LCD 显示 |
| `cst816t` | `touch/` | 触摸事件 |
| `wt588f02` | `audio/` | 语音播报 |
| `w25q64` | `externflash/` | SPI NOR，承载 LVGL 资源分区 + OTA staging |

## 依赖规则

```
Bsp_Drivers/  ──>  03_Platform/platform_mcu/  (I2C/SPI/UART/GPIO port)
Bsp_Drivers/  ──>  03_Platform/platform_os/   (OSAL，仅 handler 层；driver 层禁止)
platform_bsp wrapper 暴露给 01_App / 02_Service，不允许反向依赖
```

> **ISR 规则**：driver 层的总线操作必须由 handler 任务（线程上下文）调用，禁止在 ISR 内获取 IIC/SPI 互斥锁——通过 `osal_notify` 唤醒 handler。
