# 02_OS_Platform — OSAL（OS Abstraction Layer）

对 RTOS 内核做统一抽象，APP / Service / BSP / Middleware 只调 `osal_*` API。换 RTOS 只改 `OS_Implementation/`。

## 目录结构

```
02_OS_Platform/
├── OS_Wrapper/        ← 公开 API（osal_task / sema / mutex / queue / timer / heap / notify / event_group）
│   ├── inc/osal_*.h
│   ├── inc/osal_wrapper_adapter.h   ← 总入口，业务 include 这一个
│   └── src/osal_*.c                 ← 转调 OS_Implementation
├── OS_Implementation/ ← 当前 = FreeRTOS 映射
│   ├── inc/os_freertos.h、osal_internal_*.h、osal_macros.h、osal_config.h
│   └── src/os_impl_{task,sema,mutex,queue,timer,heap,notify,event_group}.c
└── OSAL_Common/inc/   ← 项目共享类型（osal_task_handle_t / queue_handle_t / mutex_handle_t / tick_type_t…）
```

## 当前能力

- `osal_task` — 任务创建 / 删除 / 优先级 / 栈水位
- `osal_sema` — counting / binary semaphore（含 `osal_sema_give_from_isr`）
- `osal_mutex` — 递归 / 非递归互斥锁
- `osal_queue` — FreeRTOS xQueue 包装
- `osal_timer` — 软件定时器
- `osal_heap` — heap_4 接口
- `osal_notify` — task notification（ISR → 线程唤醒首选）
- `osal_event_group` — FreeRTOS event group（storage_manager_task 用来包 async 成阻塞）

## 重要约束

- **`OSAL_SUCCESS` 与 `OSAL_FALSE` 都等于 0** —— 队列/信号量返回值禁止用 `OSAL_FALSE` 判定，存入 `int32_t` 后与 `OSAL_SUCCESS` 比较。
- 业务侧统一 include `osal_wrapper_adapter.h`，不要直接 include FreeRTOS / 实现头。

## 依赖规则

```
OS_Wrapper        ──>  OS_Implementation  (转调)
OS_Implementation ──>  FreeRTOS kernel    (本项目当前唯一实现)
```

换 RTOS = 重写 `OS_Implementation/src/os_impl_*.c` 与 `inc/os_*.h`，wrapper API 不动。
