# platform_middleware — 中间件接口占位层

预留给未来的 middleware 抽象头文件。当前为空目录，由 v5 规划接入。

## 设计意图

按 `03_Platform/` (接口) + `04_Impl/` (实现) 分树原则，中间件理论上也应该有
"接口"和"实现"的物理隔离。但当前中间件
（EasyLogger / Ymodem / heart_rate_algo / lvgl_port / LetterShell）大多
是**直接包含上游代码 + 项目适配 port**的混合体，本身就没有清晰的接口/实现
切分点：

- **EasyLogger**：上游 + `port/elog_port.c` 适配。port 是实现，没有公开
  "elog interface" 给上层选择性绑定。
- **Ymodem**：项目自有协议实现，依赖 `ota_transport_*` 抽象（这才是真正
  的接口，放在 `02_Service/service_ota/inc/`）。
- **LVGL**：上游 + lvgl_port + lvgl_ui，port 直接调 BSP，无独立接口层。
- **heart_rate_algo**：纯算法，无 OS / BSP 依赖。

所以 Phase B Part 1 阶段，所有中间件全部归位到 `04_Impl/impl_middleware/`，
本目录空置。

## v5 计划接入点

以下抽象接口将放进本目录：

| 文件 | 作用 | 解耦谁 |
|---|---|---|
| `lv_storage_port.h` | LVGL 资源存储抽象 | `lv_port_extflash.c` 不再 #include service_storage_facade.h |
| `ymodem_sink.h` | Ymodem 数据接收回调接口 | Ymodem 不再 extern OSAL queue/sema |
| `platform_log/log_if.h` | 日志**前端** facade（已落地）| Debug 层不绑 EasyLogger，可整体换日志库 |
| `platform_log/log_sink.h` | 日志**后端**传输抽象（已落地）| elog_port 不再焊死 SEGGER_RTT，可换 RTT / UART / ITM |

引入这些接口的同时会把对应实现挪到 `04_Impl/impl_middleware/` 下的专门
adapter，service 层只 include 本目录的抽象头。

## 当前状态

`platform_log/` 已落地**两层**抽象：

- **前端 facade** `log_if.h`：通用 `LOG_a/e/w/i/d/v` 宏 + `log_frontend_init()`，
  编译期选后端（默认 EasyLogger）。`Debug.h` 改 include `log_if.h`、`DEBUG_OUT`
  走 `LOG_##LEVEL`，`debug_init()` 调 `log_frontend_init()`，**不再引用 elog**。
  EasyLogger 适配器在 `04_Impl/impl_middleware/log_adapters/log_adapter_easylogger/`
  （宏映射 + 接管 elog 启动块）。**换日志库** = 加 `log_adapter_<lib>` + 在
  `log_if.h` 加 `#elif`，Debug 层 / 调用站零改动。
- **后端 sink** `log_sink.h`：transport vtable + 分发器 `log_sink.c`，RTT 适配器在
  `log_sink_rtt/`。`elog_port_output()` 调 `log_sink_write()`，`debug_init()` 在
  `log_frontend_init()` 前 `log_sink_rtt_register()`。**换 RTT/UART/ITM** = 加
  `log_sink_*` 适配器 + 改 register 一行。

启动顺序：`debug_init()` → `log_sink_rtt_register()`（先挂传输）→
`log_frontend_init()`（再起库；其 `elog_init` 内部经 `elog_port_init` 触达
`log_sink_init`）。

> 注：未抽运行时 vtable / `va_list`（换库是编译期一次性事，无需运行时可换）。
> `lv_storage_port.h` / `ymodem_sink.h` 仍待接入。
