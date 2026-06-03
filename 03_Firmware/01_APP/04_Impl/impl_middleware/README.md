# impl_middleware — 中间件实现

通用算法/协议/GUI 库。中间件之间互不耦合，按需启停；可选依赖 OSAL / BSP。

## 当前实现

| 子目录 | 内容 | 备注 |
|---|---|---|
| `EasyLogger/` | 异步日志框架 | `port/` 适配本项目；输出经 `SEGGER_RTT_SetTerminal()` 路由到 RTT 通道 0-8（详见 [`../../05_Debug_Tool/`](../../05_Debug_Tool/)） |
| `LetterShell/` | 嵌入式 CLI | **当前未启用** —— shellTask 已废弃，新功能勿引入 UART shell I/O |
| `Ymodem/` | Ymodem 收包协议 | 零 HAL include，通过 `ota_transport_*` 抽象走全链路；APP CRC-16/XMODEM 校验 + EOT 严格握手 |
| `heart_rate_algo/` | PPG 心率算法 | EM7028 frame 输入；由 `01_App/User_Sensor/em7028/` 任务驱动 |
| `lvgl/` | LVGL v8.3 + port + UI | `lvgl/` 原生库；`lvgl_port/lv_port_extflash.c` 自定义 decoder（行级 streaming 240×240 表盘背景）；`lvgl_ui/` 业务 UI |

## 依赖规则

```
EasyLogger     ──>  03_Platform/platform_os (OSAL mutex)  + 05_Debug_Tool (RTT)
Ymodem         ──>  ota_transport_* 抽象 (零 HAL；adapter 在 02_Service)
LVGL           ──>  lvgl_port → MCU_SPI (LCD) / Read_LvglData (storage_manager)
heart_rate_algo──>  无 OS / BSP 依赖，纯算法
```

## 替换

- 换 GUI 库 → 替换 `lvgl/`，重写 `lvgl_port/`
- 换日志后端 → 替换 `EasyLogger/`，保持 `DEBUG_OUT` 宏 API 不变（详见 [`../../05_Debug_Tool/`](../../05_Debug_Tool/)）
- 换 OTA 传输协议 → 替换 `Ymodem/`，service 层只依赖 `ota_transport_*` 抽象

LVGL 自定义 decoder + W25Q64 资源 bootstrap 细节见 [`../../CLAUDE.md`](../../CLAUDE.md) "外部 Flash LVGL 资源"节。
