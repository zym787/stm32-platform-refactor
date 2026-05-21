# platform_middleware — 中间件接口占位层

预留给未来的 middleware 抽象头文件。当前为空目录，由 v5 规划接入。

## 设计意图

按 `03_Platform/` (接口) + `04_Impl/` (实现) 分树原则，中间件理论上也应该有
"接口"和"实现"的物理隔离。但当前 02_Middleware_Platform/ 时代的中间件
（EasyLogger / Ymodem / heart_rate_algo / lvgl_port / LetterShell）大多
是**直接包含上游代码 + 项目适配 port**的混合体，本身就没有清晰的接口/实现
切分点：

- **EasyLogger**：上游 + `port/elog_port.c` 适配。port 是实现，没有公开
  "elog interface" 给上层选择性绑定。
- **Ymodem**：项目自有协议实现，依赖 `ota_transport_*` 抽象（这才是真正
  的接口，放在 `01_SERVICE/service_ota/inc/`）。
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
| `log_backend.h` | 日志后端抽象 | 可换 EasyLogger / RTT / UART |

引入这些接口的同时会把对应实现挪到 `04_Impl/impl_middleware/` 下的专门
adapter，service 层只 include 本目录的抽象头。

## 当前状态

空目录 + 本 README。Makefile 不引用本目录任何路径。
