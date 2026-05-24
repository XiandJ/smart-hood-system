# 智能油烟机 — 开发进度

> 最后更新：2026-05-24 | 下一阶段：Keil 工程配置 → 编译烧录 → 硬件测试

---

## 一、项目概览

| 项目 | 智能油烟机 (Smart Hood System) |
|------|-------------------------------|
| MCU | STM32H562VIT6 (Cortex-M33, 250MHz) |
| 开发板 | Mini H562 V1 |
| RTOS | FreeRTOS CMSIS_V2 |
| 工具链 | STM32CubeIDE (GCC) 或 Keil MDK-ARM |
| 设计文档 | `docs/superpowers/specs/2026-05-24-sensor-drivers-design.md` |
| 实现计划 | `docs/superpowers/plans/2026-05-24-sensor-drivers-plan.md` |

---

## 二、已完成模块 (12/12 Tasks Done)

### Task 1-2: config.h + 目录结构 ✅
- 创建了 4 个新目录，`config.h` 含 sensor_data_t 结构体和硬件参数宏

### Task 3: SHT40 驱动 ✅
- 文件：[sht40.h](project/Core/Inc/drivers/sht40.h) / [sht40.c](project/Core/Src/drivers/sht40.c)
- I2C1 地址 0x44，0xFD 高精度指令，CRC8 校验
- 接口：Init / Read(temp,hum) / GetSerial

### Task 4: Sensirion 官方库适配 + SGP40 驱动 ✅
- 8 个 Sensirion 文件放入 `Src/sensirion/` 和 `Inc/sensirion/`
- **改动1**: `sensirion_config.h` — `<stdlib.h>` 替换为 `<stdint.h>` + 手动 `#define NULL 0`
- **改动2**: 新建 `sensirion_i2c_hal.c` — read/write/sleep 三个函数适配 STM32 HAL
- SGP40 驱动封装：[sgp40.h](project/Core/Inc/drivers/sgp40.h) / [sgp40.c](project/Core/Src/drivers/sgp40.c)
- 接口：Init / MeasureRaw(temp,hum,raw) / RawToVOCIndex

### Task 5: PMS7003 驱动 ✅
- 文件：[pms7003.h](project/Core/Inc/drivers/pms7003.h) / [pms7003.c](project/Core/Src/drivers/pms7003.c)
- USART3 9600bps，GPDMA1_CH1 Circular 接收
- 3 状态帧解析器（IDLE→HDR1→DATA），IDLE 中断驱动
- `HAL_UARTEx_RxEventCallback` 喂字节 + 重启 DMA
- 回调自动生效（`USART3_IRQHandler` → `HAL_UART_IRQHandler` → 触发 callback）

### Task 6: ACS712 (Hall) 驱动 ✅
- 文件：[hall.h](project/Core/Inc/drivers/hall.h) / [hall.c](project/Core/Src/drivers/hall.c)
- ADC1_INP4 (PC4)，GPDMA1_CH2 Circular 256 采样点
- 64 点滑动窗口，Half/Full Complete 回调批量处理
- 接口：Init / GetCurrent / GetInstantCurrent / IsOvercurrent
- 临界区保护（关中断几 μs），安全支持 MotorTask 并发读取

### Task 7: 传感器融合层 + FreeRTOS 集成 ✅
- 融合层：[sensor_hub.h](project/Core/Inc/app/sensor_hub.h) / [sensor_hub.c](project/Core/Src/app/sensor_hub.c)
- 采集顺序：SHT40 → SGP40（温湿度补偿，失败用 25°C/50%RH）→ PMS7003 → ACS712
- **app_freertos.c 改动**：新增 SensorTask（优先级 osPriorityHigh, 2KB 栈, 1Hz）
- SensorTask 独占所有 I2C1 操作，无需互斥锁

### Task 8: 串口监控输出 ✅ (额外)
- 文件：[debug_console.h](project/Core/Inc/debug_console.h) / [debug_console.c](project/Core/Src/debug_console.c)
- 实现 `_write()` 重定向 printf → USART1 (PA9, 921600bps)
- **main.c 改动**：启动横幅（时钟频率 + 传感器清单）
- **Error_Handler 改动**：死循环前打印 FATAL 信息
- **assert_failed 改动**：打印文件名和行号

### Task 9: MotorTask 电机控制 ✅
- 文件：[motor.h](project/Core/Inc/drivers/motor.h) / [motor.c](project/Core/Src/drivers/motor.c)
- TIM1_CH1 (PA8) 20kHz PWM，6档调速 (0/30/45/60/80/100%)
- 软启动斜坡：500ms 从 0 到全速（RAMP_STEP = ARR/100）
- 过流保护：7.5A 软件预警 + 8A 硬急停（取绝对值）
- GPIO: MOTOR_DIR=PC5, MOTOR_EN=PD7
- MotorTask 优先级 osPriorityAboveNormal，10ms 周期 (100Hz)
- **app_freertos.c 改动**：新增 MotorTask 创建和实现
- **config.h 改动**：ACS712 参数修正 (HALL_SENSITIVITY=0.0925f, HALL_OFFSET_V=1.25f)
- **hall.c 改动**：HALL_OFFSET_V_5V → HALL_OFFSET_V

### Task 10: SystemTask 系统管理 ✅
- 文件：[system_manager.h](project/Core/Inc/app/system_manager.h) / [system_manager.c](project/Core/Src/app/system_manager.c) + [system_state.h](project/Core/Inc/system_state.h)
- 按键消抖：8-sample shift register，检测上升沿
- KEY0 (PA0): 切换 OFF ↔ AUTO
- KEY1 (PA1): 循环 MANUAL 档位 (1→2→3→4→5→0)
- WK_UP (PA15): 紧急 → OFF
- 三态状态机：SYS_OFF(灭灯) / SYS_AUTO(1Hz闪) / SYS_MANUAL(常亮)
- 蜂鸣器 (PE0): 模式切换短响，可配置时长
- AUTO 模式：读取 AI 推荐档位，调用 Motor_SetSpeed()
- Motor_ClearEmergency() 允许过流急停后恢复运行

### Task 11: AiTask AI 推理 ✅
- 文件：[ai_engine.h](project/Core/Inc/app/ai_engine.h) / [ai_engine.c](project/Core/Src/app/ai_engine.c)
- 规则引擎：PM2.5 五级阈值 + VOC 三级阈值，取较高值
- PM2.5: 35/75/115/150 → level 2/3/4/5
- VOC: 100/200/300 → level 2/3/4
- 电流 > 5A: 强制 level 5
- 输出推荐档位 (0-5)，由 SystemTask 在 AUTO 模式执行
- 1Hz 运行，2KB 栈预留 (后续 TFLite)

### Task 12: CommTask 通信框架 ✅
- 文件：[comm_task.h](project/Core/Inc/app/comm_task.h) / [comm_task.c](project/Core/Src/app/comm_task.c)
- USART1 OpenMV 通信框架（当前 stub，预留接口）
- 100ms 周期轮询

---

## 三、完整文件清单

```
project/Core/
├── Inc/
│   ├── config.h              ★ 新建  全局配置 + sensor_data_t
│   ├── system_state.h        ★ 新建  系统状态枚举
│   ├── debug_console.h       ★ 新建  串口监控接口
│   ├── main.h                         CubeMX 生成
│   ├── app_freertos.h                 CubeMX 生成
│   ├── FreeRTOSConfig.h              CubeMX 生成
│   ├── stm32h5xx_hal_conf.h          CubeMX 生成
│   ├── stm32h5xx_it.h                CubeMX 生成
│   ├── adc.h/gpdma.h/gpio.h/i2c.h    CubeMX 生成
│   ├── icache.h/iwdg.h/tim.h/usart.h CubeMX 生成
│   ├── app/
│   │   ├── sensor_hub.h      ★ 新建  融合层接口
│   │   ├── system_manager.h  ★ 新建  系统管理接口
│   │   ├── ai_engine.h       ★ 新建  AI 规则引擎接口
│   │   └── comm_task.h       ★ 新建  通信任务接口
│   ├── drivers/
│   │   ├── sht40.h           ★ 新建  SHT40 驱动
│   │   ├── sgp40.h           ★ 新建  SGP40 驱动
│   │   ├── pms7003.h         ★ 新建  PMS7003 驱动
│   │   ├── hall.h            ★ 新建  ACS712 驱动
│   │   └── motor.h           ★ 新建  电机控制驱动
│   └── sensirion/            Sensirion 官方库 (5 个 .h)
├── Src/
│   ├── main.c                ◆ 修改  启动横幅 + Error_Handler
│   ├── app_freertos.c        ◆ 修改  SensorTask + 串口输出
│   ├── stm32h5xx_it.c                CubeMX 生成 (无需改动)
│   ├── system_stm32h5xx.c            CubeMX 生成
│   ├── stm32h5xx_hal_msp.c           CubeMX 生成
│   ├── stm32h5xx_hal_timebase_tim.c  CubeMX 生成
│   ├── adc.c/gpdma.c/gpio.c/i2c.c    CubeMX 生成
│   ├── icache.c/iwdg.c/tim.c/usart.c CubeMX 生成
│   ├── app/
│   │   ├── sensor_hub.c      ★ 新建  融合层实现
│   │   ├── system_manager.c  ★ 新建  按键+状态机+蜂鸣器
│   │   ├── ai_engine.c       ★ 新建  PM2.5/VOC 规则引擎
│   │   └── comm_task.c       ★ 新建  OpenMV 通信框架
│   ├── drivers/
│   │   ├── sht40.c           ★ 新建  SHT40 实现
│   │   ├── sgp40.c           ★ 新建  SGP40 封装实现
│   │   ├── pms7003.c         ★ 新建  PMS7003 DMA+解析
│   │   ├── hall.c            ★ 新建  ACS712 ADC+窗口
│   │   └── motor.c           ★ 新建  电机 PWM+斜坡+过流
│   ├── sensirion/            Sensirion 官方库 (4 个 .c)
│   │   ├── sensirion_common.c/h     原样
│   │   ├── sensirion_i2c.c/h        原样
│   │   ├── sensirion_i2c_hal.c/h    ★ 新建 HAL 适配层
│   │   ├── sensirion_config.h       ★ 修改 去除 stdlib 依赖
│   │   └── sgp40_i2c.c/h           原样
│   └── debug_console.c       ★ 新建  printf→USART1

注：★ = 本次新建  ◆ = 本次修改
```

---

## 四、运行时启动流程

```
上电 → main()
  ├─ MPU_Config()
  ├─ HAL_Init()
  ├─ SystemClock_Config()     → 250MHz (HSE 25MHz × PLL /2*125/2)
  ├─ MX_GPIO/I2C1/ADC1/TIM1/USART1/USART3_Init()
  ├─ [printf 启动横幅]         → USART1 输出系统信息
  ├─ osKernelInitialize()
  ├─ MX_FREERTOS_Init()
  │   ├─ osThreadNew(defaultTask)
  │   ├─ osThreadNew(SensorTask, priority=High)
  │   ├─ osThreadNew(MotorTask, priority=AboveNormal)
  │   ├─ osThreadNew(AiTask, priority=AboveNormal)
  │   ├─ osThreadNew(CommTask, priority=Normal)
  │   └─ osThreadNew(SystemTask, priority=Low)
  └─ osKernelStart()
      ├─ SensorTask (1Hz):
      │   ├─ SensorHub_Init() → SHT40/SGP40/PMS7003/Hall
      │   └─ loop: SensorHub_Update → LogSensorData
      ├─ MotorTask (100Hz):
      │   ├─ Motor_Init()
      │   └─ loop: Motor_Update (ramp + overcurrent)
      ├─ AiTask (1Hz):
      │   ├─ AI_Init()
      │   └─ loop: AI_Evaluate → SetAIMotorLevel
      ├─ CommTask (10Hz):
      │   └─ loop: CommTask_Update (OpenMV stub)
      ├─ SystemTask (100Hz):
      │   └─ loop: SystemManager_Update
      │       ├─ 按键消抖 + 状态机 (OFF/AUTO/MANUAL)
      │       ├─ AUTO: Motor_SetSpeed(ai_level)
      │       ├─ 蜂鸣器 + LED 指示
      └─ defaultTask:
          └─ osDelay(1000)    → 空转
```

---

## 五、串口监控输出格式

**端口**: USART1 (PA9 TX), **波特率**: 921600, **数据位**: 8N1

```
================================================
  Smart Hood System v1.0
  MCU: STM32H562VIT6 | 250 MHz
================================================
  SHT40  (I2C1, 0x44)  ... OK
  SGP40  (I2C1, 0x59)  ... OK
  PMS7003(USART3,9600) ... OK
  ACS712 (ADC1,  PC4)  ... OK
================================================

Starting FreeRTOS kernel...

========================================
    Smart Hood Sensor Monitor
========================================
Temp(C)  Hum(%)   VOC(raw) VOC_idx PM2.5  PM10   Cur(A)  Power(W)
-------- -------- -------- ------- ------ ------ ------- --------
   25.3     52.1    15000    120      12     25    0.35      4.2
   25.4     52.0    15100    121      11     24    0.36      4.3
```

---

## 六、尚未完成的工作

### 6.1 Keil MDK 工程配置（需人工操作）
- [ ] 将 14 个新建 .c 文件加入 Keil 工程（见上方文件清单 ★ 标记）
  - `Core/Src/drivers/sht40.c`
  - `Core/Src/drivers/sgp40.c`
  - `Core/Src/drivers/pms7003.c`
  - `Core/Src/drivers/hall.c`
  - `Core/Src/drivers/motor.c`
  - `Core/Src/app/sensor_hub.c`
  - `Core/Src/app/system_manager.c`
  - `Core/Src/app/ai_engine.c`
  - `Core/Src/app/comm_task.c`
  - `Core/Src/sensirion/sensirion_common.c`
  - `Core/Src/sensirion/sensirion_i2c.c`
  - `Core/Src/sensirion/sensirion_i2c_hal.c`
  - `Core/Src/sensirion/sgp40_i2c.c`
  - `Core/Src/debug_console.c`
- [ ] 添加 3 个 include 路径:
  - `Core/Inc/drivers`
  - `Core/Inc/app`
  - `Core/Inc/sensirion`

### 6.2 其余 FreeRTOS 任务
| 任务 | 优先级 | 周期 | 职责 | 状态 |
|------|--------|------|------|------|
| SensorTask | 4 (High) | 1s | 传感器采集+串口输出 | ✅ 已实现 |
| AiTask | 3 (AboveNormal) | 1s | 规则引擎→推荐电机档位 | ✅ 已实现 |
| MotorTask | 3 (AboveNormal) | 10ms | 过流检测+PWM斜坡+调速 | ✅ 已实现 |
| CommTask | 2 (Normal) | 100ms | OpenMV 通信框架 | ✅ 已实现 (stub) |
| SystemTask | 1 (Low) | 100ms | 状态机+按键+蜂鸣器+LED | ✅ 已实现 |

### 6.3 传感器功能增强（设计文档标记为"不做什么"）
- ACS712 零点漂移自校准
- SGP40 VOC Index 精确算法（需 Sensirion VOC Algorithm 库）
- 传感器故障诊断
- 低功耗管理

---

## 七、关键技术决策（新 AI 需知晓）

1. **I2C1 共享策略**: SensorTask（优先级 4）独占所有 I2C1 操作，无需互斥锁。若将来其他任务需访问 I2C1（如 OLED），需添加 `osMutex`。

2. **SGP40 温湿度补偿兜底**: SHT40 读取失败时，用 25°C / 50%RH 作为默认值传给 SGP40。

3. **PMS7003 DMA 架构**: `HAL_UARTEx_ReceiveToIdle_DMA` + 64 字节循环缓冲。IDLE 中断触发 `HAL_UARTEx_RxEventCallback`，回调内喂字节给帧解析状态机并自动重启 DMA。无需修改 `stm32h5xx_it.c`。

4. **ACS712 线程安全**: `Hall_GetCurrent()` 和 `Hall_GetInstantCurrent()` 内部用 `__disable_irq()`/`__enable_irq()` 保护 DMA 缓冲区访问。SensorTask (1Hz) 和未来的 MotorTask (100Hz) 可安全并发调用。

5. **printf 阻塞**: `_write()` 使用 `HAL_UART_Transmit(&huart1, ..., HAL_MAX_DELAY)` 阻塞发送。USART1 速率 921600bps，每行数据约 80 字节，耗时 <1ms，对 1Hz SensorTask 无影响。若将来高频任务调用 printf，需改用 DMA 或环形缓冲。

6. **Sensirion 库零改动原则**: 仅修改 `sensirion_config.h` 的 include，其余 7 个 Sensirion 文件完全保持原样。

7. **CubeMX USER CODE 安全区**: 所有修改均在 USER CODE BEGIN/END 标记内，CubeMX 重新生成代码不会覆盖。
