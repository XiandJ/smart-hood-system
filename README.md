# 智能抽油烟机系统 (Smart Hood System)

基于 **STM32H562VIT6 + OpenMV H7 Plus** 双芯片架构的端侧AI抽油烟机控制系统。

## 系统架构

```
STM32H562VIT6 (主控, Cortex-M33@250MHz)
  ├── SHT40  (I2C) → 温湿度
  ├── SGP40  (I2C) → VOC空气质量
  ├── PMS7003(UART)→ PM1.0/2.5/10
  ├── 霍尔CT  (ADC)→ 电机电流
  ├── BLDC电机(PWM)→ 风机驱动
  ├── AI引擎 (TFLite/CMSIS-NN) → 传感器融合推理
  └── UART ←→ OpenMV H7 Plus (视觉协处理器)
                    ├── 烟雾检测 (FOMO CNN)
                    ├── 人员检测 (FOMO + MobileNetV1)
                    └── 手势识别 (MobileNetV1)
```

## 目录结构

```
smart_hood_system/
├── docs/
│   ├── 01_requirements.md          # 需求文档
│   ├── 02_system_architecture.md   # 系统架构设计
│   ├── 03_hardware_bom.md          # 硬件BOM与引脚分配
│   └── 04_ai_model_design.md       # 端侧AI模型设计
├── firmware/                        # STM32H562 固件
│   ├── Core/Inc/                    # 头文件
│   │   ├── config.h                 # 系统配置
│   │   ├── main.h                   # 全局定义
│   │   ├── sensor_hub.h             # 传感器管理中心
│   │   ├── ai_engine.h              # AI推理引擎
│   │   ├── motor_control.h          # 电机控制
│   │   ├── openmv_bridge.h          # OpenMV通信协议
│   │   └── system_manager.h         # 系统状态机
│   ├── Core/Src/                    # 源文件
│   │   ├── main.c                   # 主入口+HAL初始化
│   │   ├── sensor_hub.c             # 多传感器统一采集
│   │   ├── ai_engine.c              # AI推理+规则回退
│   │   ├── motor_control.c          # PWM+斜坡+过流保护
│   │   ├── openmv_bridge.c          # UART协议+帧解析
│   │   └── system_manager.c         # 决策融合+状态机
│   ├── Drivers/                     # 传感器驱动
│   │   ├── SHT40/
│   │   ├── SGP40/
│   │   ├── PMS7003/
│   │   └── Hall/
│   ├── Middleware/AI/
│   │   └── model.h                  # AI模型接口(占位)
│   └── CMakeLists.txt               # CMake构建
├── openmv/
│   ├── main.py                      # OpenMV主程序
│   └── models/                      # .tflite模型文件(需部署)
├── tools/
│   ├── model_training/
│   │   └── sensor_fusion_model.py   # 传感器融合模型训练
│   └── pc_simulator/
│       └── simulator.py             # PC端系统仿真器
└── README.md
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

### 1. 训练AI模型 (可选)

```bash
cd tools/model_training
pip install tensorflow scikit-learn numpy
python sensor_fusion_model.py
# 输出: sensor_fusion_model_quant.tflite
```

### 2. STM32Cube.AI 部署模型

```bash
# 将量化模型转换为C代码
stm32ai generate -m sensor_fusion_model_quant.tflite \
    -o firmware/Middleware/AI/ \
    --target stm32h5
```

### 3. 编译固件

```bash
cd firmware
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/gcc-arm-none-eabi.cmake
make -j$(nproc)
```

### 4. 部署OpenMV

将 `openmv/main.py` 和 `.tflite` 模型文件放入OpenMV SD卡。

### 5. PC仿真 (无需硬件)

```bash
cd tools/pc_simulator
python simulator.py
# 按 1/2/3/4 切换烹饪场景，观察传感器和AI推理输出
```

## 关键技术特性

- **双芯片AI**: STM32H5处理传感器融合，OpenMV处理视觉AI
- **AI降级策略**: 模型不可用时自动回退到规则引擎
- **滑动窗口特征工程**: 16秒时序数据用于趋势识别
- **多传感器融合决策**: AI推理 + 视觉数据 + 规则覆盖
- **电机软启动**: 斜坡PWM控制，过流硬件+软件双重保护
- **看门狗保护**: IWDG防止系统死锁
- **通信协议**: 自定义帧格式 + CRC16校验

## 开发阶段

| Phase | 内容 | 状态 |
|-------|------|------|
| Phase 1 | 硬件接口验证 | 设计中 |
| Phase 2 | 基础控制逻辑 | 代码就绪 |
| Phase 3 | AI模型训练 | 训练脚本就绪 |
| Phase 4 | 端侧部署 | 部署框架就绪 |
| Phase 5 | 系统集成 | 待硬件 |
| Phase 6 | 优化迭代 | 待测试 |
