# 智能抽油烟机系统 (Smart Hood System)

基于 **STM32H562VIT6 + OpenMV H7 Plus + ESP32** 三芯片架构的端侧AI抽油烟机控制系统。

## 系统架构

```
                          ┌─────────────────────────┐
                          │    Python 上位机 (PC)     │
                          │  PyQt5 + PyQtGraph       │
                          │  实时波形 / 摄像头 / 控制  │
                          └────────────┬────────────┘
                                       │ WebSocket (ws://:81)
                                       │ JSON传感器 + JPEG图像
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
└─────────────────────────┘                    │  ├── 霍尔CT → 电机电流    │
                                               │  ├── BLDC电机 → 风机驱动  │
                                               │  └── AI推理 → 自动档位    │
                                               └─────────────────────────┘
```

## 目录结构

```
smart_hood_system/
├── esp32-bridge/                    # ESP32 WiFi 桥接模块
│   ├── include/
│   │   ├── config.h                 # 引脚/网络/任务配置
│   │   └── protocol.h               # 帧协议 + CRC16
│   ├── src/
│   │   ├── main.cpp                 # FreeRTOS 双核任务调度
│   │   ├── wifi_manager.cpp/.h      # STA/AP WiFi管理
│   │   ├── uart_sensor.cpp/.h       # UART2 传感器数据接收
│   │   ├── uart_camera.cpp/.h       # UART1 JPEG图像帧解析
│   │   ├── frame_buffer.cpp/.h      # 线程安全图像环形缓冲
│   │   └── web_server.cpp/.h        # HTTP + WebSocket 服务
│   ├── host_pc/                     # Python 上位机
│   │   ├── main.py                  # 应用入口
│   │   ├── core/
│   │   │   ├── ws_client.py         # 异步WebSocket客户端
│   │   │   └── data_store.py        # 传感器数据环形缓冲
│   │   ├── ui/
│   │   │   ├── main_window.py       # 主窗口布局
│   │   │   ├── sensor_panel.py      # 传感器仪表 + 波形图
│   │   │   ├── camera_panel.py      # 摄像头实时画面
│   │   │   ├── control_panel.py     # 风机挡位控制按钮
│   │   │   └── style.py             # 暗色主题样式
│   │   └── utils/
│   │       └── logger.py            # CSV数据记录器
│   └── platformio.ini               # PlatformIO 构建配置
├── docs/
│   ├── 01_requirements.md           # 需求文档
│   ├── 02_system_architecture.md    # 系统架构设计
│   ├── 03_hardware_bom.md           # 硬件BOM与引脚分配
│   └── 04_ai_model_design.md        # 端侧AI模型设计
├── firmware/                         # STM32H562 固件
│   ├── Core/Inc/                     # 头文件
│   │   ├── config.h                  # 系统配置
│   │   ├── main.h                    # 全局定义
│   │   ├── sensor_hub.h              # 传感器管理中心
│   │   ├── ai_engine.h               # AI推理引擎
│   │   ├── motor_control.h           # 电机控制
│   │   ├── openmv_bridge.h           # OpenMV通信协议
│   │   └── system_manager.h          # 系统状态机
│   ├── Core/Src/                     # 源文件
│   │   ├── main.c                    # 主入口+HAL初始化
│   │   ├── sensor_hub.c              # 多传感器统一采集
│   │   ├── ai_engine.c               # AI推理+规则回退
│   │   ├── motor_control.c           # PWM+斜坡+过流保护
│   │   ├── openmv_bridge.c           # UART协议+帧解析
│   │   └── system_manager.c          # 决策融合+状态机
│   ├── Drivers/                      # 传感器驱动
│   │   ├── SHT40/
│   │   ├── SGP40/
│   │   ├── PMS7003/
│   │   └── Hall/
│   ├── Middleware/AI/
│   │   └── model.h                   # AI模型接口(占位)
│   └── CMakeLists.txt                # CMake构建
├── openmv/
│   ├── main.py                       # OpenMV主程序
│   └── models/                       # .tflite模型文件(需部署)
├── tools/
│   ├── model_training/
│   │   └── sensor_fusion_model.py    # 传感器融合模型训练
│   └── pc_simulator/
│       └── simulator.py              # PC端系统仿真器
└── README.md
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

### FreeRTOS 任务分配

| 任务 | 核心 | 频率 | 优先级 | 功能 |
|------|------|------|--------|------|
| sensorTask | Core 0 | 200Hz | 3 | UART2 传感器帧解析 |
| cameraTask | Core 0 | 500Hz | 4 | UART1 JPEG 图像采集 |
| networkTask | Core 1 | 100Hz | 2 | WiFi + WebSocket 推送 |

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
- **全屏模式**: F11 切换，适合比赛展示

### 启动上位机

```bash
cd esp32-bridge/host_pc
pip install -r requirements.txt
python main.py
```

## 端侧AI方案

### 1. STM32H562 传感器融合模型 (规则引擎可用 + AI模型可选)

| 属性 | 值 |
|------|-----|
| 输入 | 8维传感器特征 × 16秒时间窗口 |
| 架构 | Conv1D → GlobalAvgPool → Dense |
| 参数量 | ~3,200 |
| 量化后大小 | ~4KB (int8) |
| 推理延迟 | <15ms (CMSIS-NN/DSP) |
| 输出 | 风机档位(0-5) + 空气质量等级(4类) |

- **AI模型可用时**: 使用 int8 量化 TFLite 模型推理 (通过 STM32Cube.AI 部署)
- **AI模型不可用时**: 自动回退到规则引擎 (多阈值加权评分)

### 2. OpenMV 视觉AI (3个模型并行)

| 模型 | 任务 | 架构 | FPS |
|------|------|------|-----|
| 烟雾检测 | 实时烟雾识别 | FOMO (64×64 gray) | ~30 |
| 人员检测 | 灶前人员检测 | FOMO + MobileNetV1 | ~15 |
| 手势识别 | 隔空手势控制 | MobileNetV1 0.25 | ~25 |

## 快速开始

### 1. ESP32 固件烧录

```bash
cd esp32-bridge
pio run -t upload --upload-port COMx
```

### 2. 启动上位机

```bash
cd esp32-bridge/host_pc
pip install -r requirements.txt
python main.py
```

### 3. WiFi 连接

- ESP32 上电后自动进入 AP 模式: `ESP32-SmartHood` (密码: `smarthood123`)
- 电脑连接该 WiFi，在上位机输入 `192.168.4.1` 点击连接
- 也可配置为 STA 模式连接已有路由器 (修改 `config.h` 中的 `WIFI_SSID`)

### 4. 训练AI模型 (可选)

```bash
cd tools/model_training
pip install tensorflow scikit-learn numpy
python sensor_fusion_model.py
```

### 5. PC仿真 (无需硬件)

```bash
cd tools/pc_simulator
python simulator.py
```

## 关键技术特性

- **三芯片协同**: STM32传感器融合 + OpenMV视觉AI + ESP32 WiFi桥接
- **双核并行**: ESP32 Core 0 采集数据, Core 1 网络通信, 互不阻塞
- **AI降级策略**: 模型不可用时自动回退到规则引擎
- **WiFi自动降级**: STA 连接失败自动切换 AP 模式
- **实时推送上位机**: 传感器5Hz + 图像10fps, WebSocket 双向通信
- **滑动窗口特征工程**: 16秒时序数据用于趋势识别
- **电机软启动**: 斜坡PWM控制，过流硬件+软件双重保护
- **通信协议**: 自定义帧格式 + CRC16校验

## 开发阶段

| Phase | 内容 | 状态 |
|-------|------|------|
| Phase 1 | 硬件接口验证 | 设计中 |
| Phase 2 | 基础控制逻辑 | 代码就绪 |
| Phase 3 | AI模型训练 | 训练脚本就绪 |
| Phase 4 | 端侧部署 | 部署框架就绪 |
| Phase 5 | ESP32 WiFi桥接 | **已完成** |
| Phase 6 | Python上位机 | **已完成** |
| Phase 7 | 系统集成 | 待硬件 |
| Phase 8 | 优化迭代 | 待测试 |
