# platform_common — 公共平台类型

`03_Platform/` 系列里的基底层，给 OS / MCU / BSP / Middleware 四个 platform，以及上面的 APP / Service 提供共同的类型与错误码词汇表。

## 文件

| 文件 | 内容 |
|---|---|
| `inc/platform_type.h` | 基础类型出口：stdint.h 别名 + `float32_t`/`float64_t`/`bool_t` |
| `inc/platform_error.h` | `platform_err_t` 枚举 + `PLATFORM_IS_OK()` / `PLATFORM_IS_ERR()` |
| `inc/platform_def.h` | NULL / 对齐 / `ARRAY_SIZE` / `MIN`/`MAX` / `UNUSED` 等通用宏 |

## 引入方式

```c
#include "platform_type.h"     // 上层只 include 这个，不直接 #include <stdint.h>
#include "platform_error.h"    // 需要返回错误码的接口才 include
#include "platform_def.h"      // 需要 ARRAY_SIZE / 对齐 / MIN 的实现文件
```

## 边界

- **不放** delay / sleep / tick —— OS（osal_task_delay）和 MCU（systick）各自负责。
- **不放** 日志/断言宏 —— 在 `05_Debug_Tool/`。
- **不放** OSAL_* —— `platform_err_t` 是新写代码的统一错误码，OSAL_* 不强制迁移；两者数值上 `OSAL_SUCCESS == PLATFORM_OK == 0`，可在 service 边界相互转换。

## 为什么不直接用 stdint.h

短期看完全可以。引入 `platform_type.h` 是为了：

1. 给后续 middleware 抽象（接口/实现分离）一个稳定的类型来源——`platform_*` 抽象头只依赖本文件，不再分别 `#include <stdint.h>` 散落到上百个文件。
2. 万一未来换工具链（如 arm-clang / IAR），只改本文件即可。
3. 对齐 `04-platform-common-class/` 教学骨架的命名习惯，方便后续与那条线合流。

## 与 04-platform-common-class 的对应

| 教学骨架 | 本工程 |
|---|---|
| `04_Impl/impl_board/board_types.h` | 跳过——直接用 stdint.h（项目早期已铺开） |
| `03_Platform/platform_common/platform_type.h` | `03_Platform/platform_common/inc/platform_type.h` |
| `03_Platform/platform_common/platform_error.h` | `03_Platform/platform_common/inc/platform_error.h` |
| `03_Platform/platform_common/platform_def.h` | `03_Platform/platform_common/inc/platform_def.h` |
