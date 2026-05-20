# 智能抽油烟机系统 - 系统架构

## 1. 硬件拓扑

```
┌─────────────────────────────────────────────────────────┐
│                    STM32H562VIT6 (主控)                   │
│  Cortex-M33 @250MHz, 1MB Flash, 384KB SRAM               │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐ │
│  │ I2C1     │  │ I2C1     │  │ USART2   │  │ ADC1    │ │
│  │ SHT40    │  │ SGP40    │  │ PMS7003  │  │ Hall CT │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘ │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐              │
│  │ TIM1 CH1 │  │ USART1   │  │ IWDG     │              │
│  │ PWM→MOS  │  │→OpenMV   │  │ WDT      │              │
│  └──────────┘  └──────────┘  └──────────┘              │
│                                                          │
│  ┌──────────────────────────────────────┐               │
│  │        AI Engine (CMSIS-NN/DSP)       │               │
│  │   Sensor Fusion MLP (TFLite Micro)    │               │
│  └──────────────────────────────────────┘               │
└────────────────────┬────────────────────────────────────┘
                     │ UART1 (921600bps)
┌────────────────────┴────────────────────────────────────┐
│              OpenMV (STM32H743VI)                        │
│  Cortex-M7 @480MHz, 2MB Flash, 1MB SRAM                  │
│                                                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ Smoke Detect │  │ Person Detect│  │ Gesture Recog│  │
│  │ (FOMO/CNN)   │  │ (FOMO/Mobile │  │ (Custom CNN) │  │
│  │              │  │  NetV1)      │  │              │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
```

## 2. 软件架构 (分层设计)

```
┌───────────────────────────────────────────────────────┐
│                   Application Layer                     │
│  system_manager.c  (状态机、模式切换、任务调度)          │
├───────────────────────────────────────────────────────┤
│                   Middleware Layer                       │
│  ai_engine.c       (TFLite Micro推理、特征工程)         │
│  openmv_bridge.c   (通信协议、数据解析、命令封装)        │
├───────────────────────────────────────────────────────┤
│                   Service Layer                          │
│  sensor_hub.c      (多传感器统一管理、数据融合)          │
│  motor_control.c   (PID控制、PWM驱动、过流保护)          │
│  filter_life.c     (滤网寿命预测算法)                   │
├───────────────────────────────────────────────────────┤
│                   Driver Layer                           │
│  sht40.c, sgp40.c, pms7003.c, hall.c                    │
├───────────────────────────────────────────────────────┤
│                   HAL Layer (STM32Cube)                  │
│  I2C, UART, ADC, TIM, GPIO, DMA                        │
└───────────────────────────────────────────────────────┘
```

## 3. 数据流

```
Sensor Loop (1Hz):
  SHT40 ──→ I2C ──→ sht40_read() ──→ sensor_hub.buffer
  SGP40 ──→ I2C ──→ sgp40_read() ──→ sensor_hub.buffer
  PMS7003 → UART → pms7003_parse() → sensor_hub.buffer
  Hall CT → ADC ──→ hall_read() ────→ sensor_hub.buffer

AI Loop (1Hz):
  sensor_hub.buffer[8] ──→ feature_engineering() ──→ model_input[64]
                                                       │
  model_input[64] ──→ tflite_invoke() ──→ {fan_level(0-5), aq_class(0-3)}
                                                     │
                                                     ▼
  motor_control.set_target(fan_level) ──→ PID ──→ PWM duty
                                              │
                                              ▼
  Hall CT ←── current_feedback ──→ overcurrent_check()

Vision Loop (~5Hz):
  OpenMV Camera ──→ smoke_detect(model) ──→ smoke_level[0-100]
                ──→ person_detect(model) ──→ person_present[bool]
                ──→ gesture_recognize(model) ──→ gesture_cmd
                                                     │
                  UART Pack ─────────────────────→ openmv_bridge.parse()
                                                     │
  openmv_bridge.data ──→ system_manager.fuse_with_sensors()
```

## 4. 状态机设计

```
States: OFF → STANDBY → RUNNING → COOLDOWN → OFF
                ↑          │           │
                │          ↓           │
                └──── ALERT (any sensor fault)

Transitions:
  OFF → STANDBY:   Power ON / Person detected
  STANDBY → RUNNING: Smoke/PM detected above threshold OR manual ON
  STANDBY → OFF:    No person for 3 minutes
  STANDBY → ALERT:  Sensor initialization failure
  RUNNING → STANDBY: Smoke/PM below threshold for 30s AND no person
  RUNNING → COOLDOWN: Manual OFF or auto-off timer
  RUNNING → ALERT: Motor overcurrent / sensor fault
  COOLDOWN → OFF:   Motor stopped + 60s delay
  ALERT → STANDBY:  Fault cleared (manual reset)
```

## 5. 内存规划

| 区域 | 用途 | 大小 |
|------|------|------|
| DTCM (128KB) | AI模型张量、传感器缓冲 | ~64KB |
| SRAM1 (128KB) | 系统堆栈、任务上下文 | ~32KB |
| SRAM2 (64KB) | DMA缓冲 (UART/I2C) | ~8KB |
| SRAM3 (64KB) | 通信帧缓冲 | ~16KB |
| Flash (1MB) | 代码 + AI模型权重 | ~512KB |
