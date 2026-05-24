# Smart Hood System

[![STM32](https://img.shields.io/badge/MCU-STM32H562VIT6-blue)](https://www.st.com/en/microcontrollers-microprocessors/stm32h562vit6.html)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS_CMSIS_V2-green)](https://www.freertos.org/)
[![License](https://img.shields.io/badge/License-MIT-lightgrey)](LICENSE)

基于 **STM32H562VIT6 + OpenMV H7 Plus + ESP32** 三芯片架构的端侧AI智能抽油烟机控制系统。

## 系统架构

```
                          ┌─────────────────────────┐
                          │    Python 上位机 (PC)     │
                          │  PyQt5 + PyQtGraph       │
                          │  实时波形 / 摄像头 / 控制  │
                          └────────────┬────────────┘
                                       │ WebSocket (ws://:81)
                          ┌────────────┴────────────┐
                          │   ESP32-WROOM-32E       │
                          │   WiFi Bridge 桥接模块    │
                          │  UART1 ← OpenMV 图像     │
                          │  UART2 ← STM32 传感器     │
                          └────┬──────────────┬─────┘
                               │              │
              ┌────────────────┘              └────────────────┐
              ▼                                                ▼
┌─────────────────────────┐                    ┌─────────────────────────┐
│  OpenMV H7 Plus         │                    │  STM32H562VIT6 (主控)    │
│  视觉协处理器             │◄──── UART ────────│  Cortex-M33 @ 250MHz    │
│  ├── 烟雾检测 (FOMO)     │                    │  ├── SHT40 → 温湿度      │
│  ├── 人员检测 (FOMO+MBV1)│                    │  ├── SGP40 → VOC         │
│  └── 手势识别 (MBV1)     │                    │  ├── PMS7003 → PM2.5     │
└─────────────────────────┘                    │  ├── ACS712 → 电机电流    │
                                               │  ├── DRV8833 → 风机驱动  │
                                               │  └── AI推理 → 自动档位    │
                                               └─────────────────────────┘
```

## 硬件平台

| 项目 | 规格 |
|------|------|
| MCU | STM32H562VIT6 (Cortex-M33, 250MHz) |
| 开发板 | Mini H562 V1 (LQFP-100) |
| HSE | 8 MHz → PLL → 250 MHz SYSCLK |
| RTOS | FreeRTOS CMSIS_V2 |
| 工具链 | Keil MDK-ARM / STM32CubeIDE |

### 引脚分配

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| SHT40/SGP40 SCL | PB6 | I2C1 (400kHz) | 温湿度 + VOC |
| SHT40/SGP40 SDA | PB7 | I2C1 | 地址 0x44 / 0x59 |
| PMS7003 TX/RX | PD8/PD9 | USART3 (9600) | PM2.5 粉尘 |
| ACS712 VOUT | PC4 | ADC1_INP4 | 5V供电 + 1:2分压 |
| Motor PWM | PA8 | TIM1_CH1 (20kHz) | DRV8833 电机驱动 |
| Motor DIR | PC5 | GPIO OUT | 方向控制 |
| Motor EN | PD7 | GPIO OUT | 使能 |
| OpenMV TX/RX | PA9/PA10 | USART1 (921600) | 视觉协处理器 |
| KEY0/KEY1 | PA0/PA1 | GPIO IN (PU) | 模式/档位按键 |
| WK_UP | PA15 | GPIO IN (PD) | 电源按键 |
| Buzzer | PE0 | GPIO OUT | 板载蜂鸣器 |
| Debug LED | PC13 | GPIO OUT | 板载LED |

## 固件架构

### 目录结构

```
project/
├── Core/
│   ├── Inc/
│   │   ├── config.h                  # 全局配置 + sensor_data_t
│   │   ├── system_state.h            # 系统状态枚举 (OFF/AUTO/MANUAL)
│   │   ├── debug_console.h           # printf → USART1
│   │   ├── main.h                    # CubeMX 生成
│   │   ├── app/
│   │   │   ├── sensor_hub.h          # 传感器融合层
│   │   │   ├── ai_engine.h           # AI 规则引擎
│   │   │   ├── system_manager.h      # 系统管理 (按键+状态机)
│   │   │   └── comm_task.h           # OpenMV 通信框架
│   │   ├── drivers/
│   │   │   ├── sht40.h               # SHT40 温湿度
│   │   │   ├── sgp40.h               # SGP40 VOC
│   │   │   ├── pms7003.h             # PMS7003 PM2.5
│   │   │   ├── hall.h                # ACS712 电流检测
│   │   │   └── motor.h               # 电机控制
│   │   └── sensirion/                # Sensirion 官方 I2C 库
│   ├── Src/
│   │   ├── main.c                    # 主入口 + 启动横幅
│   │   ├── app_freertos.c            # 5 个 FreeRTOS 任务创建
│   │   ├── debug_console.c           # _write() → USART1
│   │   ├── app/                      # 应用层
│   │   ├── drivers/                  # 传感器/电机驱动
│   │   └── sensirion/                # Sensirion 库 + HAL 适配
│   └── ...
├── Drivers/                          # STM32 HAL + CMSIS
├── Middlewares/                       # FreeRTOS 源码
├── MDK-ARM/                          # Keil 工程文件
└── chuangganqi.ioc                   # CubeMX 配置
```

### FreeRTOS 任务

| 任务 | 优先级 | 周期 | 栈 | 职责 |
|------|--------|------|-----|------|
| SensorTask | High | 1s | 2KB | 传感器采集 + 串口输出 |
| AiTask | AboveNormal | 1s | 8KB | 规则引擎 → 电机推荐档位 |
| MotorTask | AboveNormal | 10ms | 1KB | PWM 斜坡 + 过流保护 |
| CommTask | Normal | 100ms | 2KB | OpenMV 通信 (预留) |
| SystemTask | Low | 100ms | 1KB | 按键 + 状态机 + 蜂鸣器 |

### 传感器驱动

| 传感器 | 接口 | 驱动接口 |
|--------|------|---------|
| SHT40 (温湿度) | I2C1, 0x44 | `SHT40_Init()` / `SHT40_Read(&temp, &hum)` |
| SGP40 (VOC) | I2C1, 0x59 | `SGP40_Init()` / `SGP40_MeasureRaw(temp, hum, &raw)` |
| PMS7003 (PM2.5) | USART3, 9600 | `PMS7003_Init()` / `PMS7003_GetData(&data)` |
| ACS712 (电流) | ADC1_INP4, PC4 | `Hall_Init()` / `Hall_GetCurrent()` / `Hall_GetInstantCurrent()` |

### 电机控制

- TIM1_CH1 (PA8), 20kHz PWM, 6 档调速 (0/30/45/60/80/100%)
- 软启动斜坡: ~500ms 从 0 到目标占空比
- 过流保护: |I| > 8A 立即急停 (PWM 停止 + EN 拉低)
- `Motor_SetSpeed(level)` / `Motor_EmergencyStop()` / `Motor_ClearEmergency()`

### 系统状态机

```
        KEY0 短按                KEY1 按下
  ┌────────────────┐     ┌──────────────────┐
  │                ▼     │                  ▼
┌──────┐       ┌──────┐ │  ┌──────────┐   ┌──────────┐
│ OFF  │◄──────│ AUTO │◄┘  │ MANUAL   │──►│ MANUAL   │
│ 灭灯 │ KEY0  │ 闪灯 │    │ 档位 1   │   │ 档位 2-5 │
└──────┘       └──────┘    └──────────┘   └──────────┘
  ▲                          常亮              常亮
  │ WK_UP
  └─────────────────────────────────────────────┘
```

- **KEY0 (PA0)**: OFF ↔ AUTO 切换
- **KEY1 (PA1)**: 循环 MANUAL 档位 (1→2→3→4→5→0)
- **WK_UP (PA15)**: 任意模式 → OFF (紧急关机)
- AUTO 模式: AI 引擎根据 PM2.5/VOC 阈值自动推荐电机档位

## 串口监控

**端口**: USART1 (PA9 TX), **波特率**: 921600, 8N1

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

## ESP32 WiFi 桥接模块

ESP32-WROOM-32E 作为独立 WiFi 桥接模块，通过 UART 接收传感器和图像数据，经 WebSocket 实时推送到上位机。

### 通信协议

| 方向 | 格式 | 说明 |
|------|------|------|
| STM32 → ESP32 | UART 115200, 帧协议+CRC16 | 传感器数据 (温湿度/VOC/PM2.5/电流/挡位) |
| OpenMV → ESP32 | UART 921600, JPEG SOI/EOI | QVGA 320x240 JPEG 图像帧 |
| ESP32 → 上位机 | WebSocket 文本帧 | JSON 传感器数据 @5Hz |
| ESP32 → 上位机 | WebSocket 二进制帧 | JPEG 图像 @10fps |
| 上位机 → ESP32 | WebSocket 文本帧 | JSON 控制命令 `{type:"control", fan:0-5}` |

### 资源占用

- **RAM**: 22.4% (73KB / 327KB)
- **Flash**: 61.5% (806KB / 1.3MB)

## Python 上位机

基于 PyQt5 + PyQtGraph 的桌面控制台，用于比赛展示和调试。

- **传感器面板**: 6个实时仪表卡片 + 2分钟滚动波形图
- **摄像头面板**: OpenMV JPEG 实时画面显示
- **控制面板**: 6挡风机按钮 + 键盘快捷键 + 紧急停止
- **报警指示**: PM2.5 > 75 或 VOC > 100 卡片变红闪烁
- **数据记录**: 自动 CSV 日志导出

## AI 方案

### STM32 规则引擎 (当前实现)

| 传感器 | 阈值 | 推荐档位 |
|--------|------|---------|
| PM2.5 | < 35 μg/m³ | 0-1 |
| PM2.5 | 35-75 μg/m³ | 2 |
| PM2.5 | 75-115 μg/m³ | 3 |
| PM2.5 | 115-150 μg/m³ | 4 |
| PM2.5 | ≥ 150 μg/m³ | 5 |
| VOC Index | < 100 | 0-1 |
| VOC Index | 100-200 | 2 |
| VOC Index | 200-300 | 3 |
| VOC Index | ≥ 300 | 4 |
| 电流 | > 5A | 强制 5 |

PM2.5 和 VOC 取较高值作为推荐档位，由 SystemTask 在 AUTO 模式执行。

### TFLite 模型 (预留)

| 属性 | 值 |
|------|-----|
| 输入 | 8维传感器特征 × 16秒时间窗口 |
| 架构 | Conv1D → GlobalAvgPool → Dense |
| 参数量 | ~3,200 |
| 量化后大小 | ~4KB (int8) |
| 推理延迟 | <15ms (CMSIS-NN/DSP) |
| 输出 | 风机档位(0-5) + 空气质量等级(4类) |

## 快速开始

### 1. STM32 固件编译

1. 用 Keil MDK-ARM 打开 `project/MDK-ARM/chuangganqi.uvprojx`
2. 将以下文件添加到工程 (如尚未添加):
   - `Core/Src/drivers/sht40.c` `sgp40.c` `pms7003.c` `hall.c` `motor.c`
   - `Core/Src/app/sensor_hub.c` `system_manager.c` `ai_engine.c` `comm_task.c`
   - `Core/Src/sensirion/sensirion_common.c` `sensirion_i2c.c` `sensirion_i2c_hal.c` `sgp40_i2c.c`
   - `Core/Src/debug_console.c`
3. 添加 Include 路径: `Core/Inc/drivers` `Core/Inc/app` `Core/Inc/sensirion`
4. 编译烧录

### 2. CubeMX 重新生成

如需修改引脚或外设配置，用 CubeMX 打开 `project/chuangganqi.ioc`，按 `docs/` 下的配置手册操作。所有用户代码均在 `USER CODE BEGIN/END` 安全区内。

### 3. ESP32 固件烧录

```bash
cd esp32-bridge
pio run -t upload --upload-port COMx
```

### 4. 启动上位机

```bash
cd esp32-bridge/host_pc
pip install -r requirements.txt
python main.py
```

## 关键技术特性

- **三芯片协同**: STM32 传感器融合 + OpenMV 视觉 AI + ESP32 WiFi 桥接
- **5 任务 FreeRTOS**: Sensor/Motor/Ai/Comm/System 各司其职
- **AI 降级策略**: 规则引擎可用，后续可替换为 TFLite 模型
- **电机软启动**: 斜坡 PWM 控制，过流硬件+软件双重保护
- **传感器融合**: SHT40 温湿度补偿 SGP40，失败自动使用默认值
- **线程安全**: ADC DMA 临界区保护，支持 SensorTask + MotorTask 并发
- **CubeMX 兼容**: 所有用户代码在安全区内，重新生成不覆盖

## 开发进度

| 模块 | 状态 |
|------|------|
| SHT40 驱动 | **已完成** |
| SGP40 驱动 + Sensirion 库 | **已完成** |
| PMS7003 驱动 | **已完成** |
| ACS712 驱动 | **已完成** (需硬件校准) |
| 电机控制 (MotorTask) | **已完成** |
| 传感器融合层 | **已完成** |
| AI 规则引擎 (AiTask) | **已完成** |
| 系统状态机 (SystemTask) | **已完成** |
| 串口调试 (DebugConsole) | **已完成** |
| OpenMV 通信 (CommTask) | 框架就绪 (stub) |
| ESP32 WiFi 桥接 | **已完成** |
| Python 上位机 | **已完成** |
| TFLite 模型部署 | 待训练 |
| 硬件集成测试 | 待烧录 |

## 许可证

MIT License
