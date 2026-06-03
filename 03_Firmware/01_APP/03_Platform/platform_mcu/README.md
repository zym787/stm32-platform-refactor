# platform_mcu — MCU 端口抽象

把"芯片相关"的所有触碰收在这一层（HAL 调用、寄存器、ISR、PRIMASK 关中断）。BSP / Service / APP 一律通过 `mcu_*_port.h` 的稳定 API 操作硬件；换 MCU 只改这一层。

## 子模块

| 目录 | 职责 |
|---|---|
| `MCU_Core_IIC/i2c_port/` | I2C 总线 port。同时支持硬件 I2C（HAL）和软件位操作（SCL=PB14 / SDA=PB15）+ 互斥锁 |
| `MCU_Core_SPI/spi_port/` | SPI 总线 port（SPI1 LCD / SPI2 W25Q64）+ 互斥锁 |
| `MCU_Core_UART/uart_port/` | UART port（`uart_id` 参数化，IT / DMA-idle 双模式 + 通用 ISR dispatch；OTA UART1 走这层） |
| `MCU_Core_GPIO/` | 引脚配置 / EXTI |
| `MCU_Core_Systick/` | SysTick + HAL tick |
| `MCU_Core_DWT/` | DWT 高精度 µs 计时 |
| `MCU_Core_IFlash/` | 内部 Flash 擦写：`mcu_iflash_erase_sector` / `program_words`，函数体内 `__disable_irq()` 包裹整段（防 F411 单 bank flash 取指死锁） |
| `MCU_Core_Watchdog/` | IWDG 喂狗（F411 IWDG 起后不可关，`iwdg_feeder_task` 在 service 层 500 ms 调一次） |

## 关键约束

- **F411 内部 Flash 操作必须关中断**：erase 阻塞 ~1 s，期间 ISR 在 flash 取指会死锁。IFlash port 内部已用 `__disable_irq()` 兜底，service / APP 调用方无需再关。
- **UART RX 不要用 polled HAL**：`HAL_UART_Receive` 阻塞自旋会饿死 `PRI_BACKGROUND` 任务（IWDG 会 fire）；OTA / 业务侧请走 IT 或 DMA-idle + 信号量。
- **总线互斥锁不能进 ISR**：所有 I2C/SPI 操作必须由线程上下文调用，ISR 通过 `osal_notify` 唤醒 handler 任务。

## 依赖规则

```
MCU_Core_*  ──>  ST HAL / CMSIS / 寄存器
            ──>  03_Platform/platform_os/  (osal_sema / osal_mutex，仅在 *_port 公开 API 内部)
```

不依赖 `01_App/`、`02_Service/`、`03_Platform/platform_bsp/`。
