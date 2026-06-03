# 01_App — 应用业务层

业务任务、初始化流程、ISR 派发、任务表登记。**只调用平台抽象 API**（OSAL / BSP wrapper / Middleware），不直接碰 HAL / 寄存器 / 具体外设驱动。

完整架构、ISR 规则、新增外设步骤见 [../CLAUDE.md](../CLAUDE.md)。

## 目录结构

| 子目录 | 内容 |
|---|---|
| `User_Init/` | `user_init.c` 启动入口；遍历 `g_user_task_cfg[]` 创建任务；`Platform_IO_Register/` 把硬件 IO 绑定到驱动 adapter |
| `User_Task_Config/` | `g_user_task_cfg[]` 任务表（名称/栈/优先级/入口/参数）+ `task_higher_water_monitor.c` 运行时栈水位监控 |
| `User_Isr_handlers/` | ISR 上下文派发：`osal_notify` 唤醒线程，**禁止**在 ISR 内取 IIC 互斥锁 |
| `User_Sensor/` | 业务侧传感器/外设消费任务：`temp_humi/`、`mpu6050/`、`em7028/`、`audio/`、`display/`、`touch/`、`storage/`（LVGL 资源 bootstrap + 阻塞门面） |

> `User_OtaManager/` 当前为空 —— OTA 升级链路已迁移到 [`../02_Service/service_ota/`](../02_Service/service_ota/)。

## 任务优先级（`User_Task_Config/inc/user_task_reso_config.h`）

`PRI_EMERGENCY` > `PRI_HARD_REALTIME` > `PRI_SOFT_REALTIME` > `PRI_NORMAL` > `PRI_BACKGROUND`

新增任务 = 在 `g_user_task_cfg[]` 添一项，无需手写 `osal_task_create`。

## 依赖规则

```
01_App/  ──>  03_Platform/platform_os/   (OSAL API)
         ──>  03_Platform/platform_bsp/  (vtable wrapper)
         ──>  04_Impl/impl_middleware/   (LVGL / Log / Ymodem)
         ──>  05_Common_Utils/           (FLM / 后续通用工具)
         ──>  00_Config/                 (CFG_ 宏)
```

**反向依赖被禁止**：service / BSP / MCU / Middleware 不允许 include `01_App/` 任何头文件。
