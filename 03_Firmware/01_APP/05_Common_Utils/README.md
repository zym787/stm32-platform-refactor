# 04_Common_Utils — 通用工具

硬件/业务无关的辅助产物。**不依赖** OSAL / BSP / MCU，所有层都可复用（或在烧录链路里复用）。

## 当前内容

| 子目录 | 内容 |
|---|---|
| `01_Flash_Algorithm/W25Q64_8M_FLM.FLM` | 自定义 JLink Flash 算法（Keil MDK 工程编出），把 W25Q64 SPI bank 挂在 JLink 虚拟地址 `0x90000000`。FLM 内部把 JLink 地址重映射到 W25Q64 物理 `0x300000`（LVGL 分区起点）——`JFlash` / `Ozone` 工具链**只能**写 LVGL 分区，无法误伤 OTA / FlashDB / FATFS。`make flash-assets` 走这个 FLM。源码工程在仓库根的 `std_program_algorithms/`。 |

## 部署位置

`W25Q64_8M_FLM.FLM` 需复制到 `%APPDATA%\SEGGER\JLinkDevices\ST\STM32F4\`，并在同目录 `Devices.xml` 注册自定义设备 `STM32F411CE_W25Q64`。详见 [`../CLAUDE.md`](../CLAUDE.md) "外部 Flash LVGL 资源 / 烧录工具链"节。

## 扩展方向

后续可在此放：

- `StrUtils/` — 字符串处理
- `CRC16/` — 校验（Ymodem 已内置 CRC-16/XMODEM；如复用提到此处）
- `MemPool/` — 静态内存池
- `ByteConverter/` — 字节序 / 打包

> 加新模块前确认它确实"硬件 / 业务无关"——和 OS 或 BSP 有耦合的请放回对应平台层。
