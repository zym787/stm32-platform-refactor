# 00_Config — 项目级配置

`CFG_` 前缀的项目宏开关、地址/大小常量、状态字定义的**唯一真源**。APP、Service、Bootloader、Tools 全部从这里取值，避免散落硬编码导致不一致。

## 文件

| 文件 | 内容 |
|---|---|
| `inc/cfg_storage.h` | W25Q64 分区布局：LVGL 分区起点 `MEMORY_LVGL_START_ADDRESS=0x300000`、OTA staging 起点 `MEMORY_OTA_START_ADDRESS=0x000000`、各 LVGL 资源 offset/size（fen / miao / time / biaopan1）+ magic `0xA55A5AA5` |
| `inc/cfg_ota.h` | OTA flag 持久化结构 `ota_flag_t` + magic `0xA55A5AA5` + 状态宏（`CFG_OTA_INIT_NO_APP=0xFF` / `NO_APP_UPDATE=0x00` / `DOWNLOAD_FINISHED=0x22` / `APP_CHECK_START=0x33` / `APP_CHECKING=0x44`）+ flag 地址 `CFG_OTA_FLAG_ADDRESS=0x08008000` |

## 一致性约束

`cfg_ota.h` 必须和 Bootloader 侧 `00_Bootloader/Tasks/Bootmanager/inc/ota_flag.h` **字节兼容**——结构体布局、magic、状态值要逐字段对齐。改一边就同步改另一边并验证。

`cfg_storage.h` 与 `Tools/pack_assets.py`、`std_program_algorithms/W25Q64_8M_FLM` 共享 LVGL 分区起点；改这个值要同时校 FLM 重映射 (`adr - 0x90000000 + 0x300000`) 和 storage_manager 的 `addr + MEMORY_LVGL_START_ADDRESS`。

## 命名规范

- 宏统一用 `CFG_` 前缀
- 按模块归类：`CFG_OTA_*`、`MEMORY_*`、`LVGL_*`
- **不放业务参数**：业务侧任务栈/优先级在 `01_App/User_Task_Config/`，板级 IO 在 `Core/Inc/main.h`

## 依赖规则

只放常量与结构体，不依赖任何运行时代码，所有层都可 include。
