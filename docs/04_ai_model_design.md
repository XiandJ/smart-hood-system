# 智能抽油烟机系统 - AI模型设计

## 1. 端侧AI总体策略

```
┌─────────────────────────────────────────────────────┐
│                    Dual-Chip AI                       │
│                                                       │
│  STM32H562 (Cortex-M33 + DSP)                        │
│  ├── 传感器融合MLP (多模态 → 空气质量决策)             │
│  └── 异常检测自编码器 (电机电流异常)                   │
│                                                       │
│  OpenMV H7 Plus (Cortex-M7)                          │
│  ├── FOMO: 烟雾检测 (64x64 → smoke_level)            │
│  ├── FOMO: 人员检测 (96x96 → person_bbox)            │
│  └── MobileNetV1: 手势识别 (96x96 → gesture_class)   │
└─────────────────────────────────────────────────────┘
```

## 2. 传感器融合模型 (STM32H562)

### 2.1 模型架构

```
Input: [batch=1, seq_len=16, features=8]
       8 features: [T, RH, VOC, PM1.0, PM2.5, PM10, I_motor, dI/dt]

┌────────────────────────────────────────┐
│ Conv1D(filters=16, kernel=3, ReLU)     │
│ MaxPool1D(pool=2)                       │
├────────────────────────────────────────┤
│ Conv1D(filters=32, kernel=3, ReLU)     │
│ MaxPool1D(pool=2)                       │
├────────────────────────────────────────┤
│ GlobalAveragePooling1D                  │
├────────────────────────────────────────┤
│ Dense(16, ReLU)                        │
│ Dropout(0.2)                           │
│ Dense(6, Softmax)  → fan_level [0-5]   │
├────────────────────────────────────────┤
│ Aux: Dense(4, Softmax) → aq_class      │
└────────────────────────────────────────┘

Total params: ~3,200
Model size (int8 quantized): ~4KB
Inference time (Cortex-M33 @250MHz): ~15ms
```

### 2.2 训练策略

- 数据集: 模拟厨房环境，采集1万+条标注数据
- 标注: 人工标注风机档位 (0-5) 对应不同烹饪场景
- 量化: 训练后int8量化 (TFLite Converter)
- 部署: STM32Cube.AI 或 X-CUBE-AI 转换为C代码

## 3. OpenMV视觉模型

### 3.1 烟雾检测 (FOMO)

```
Model: Edge Impulse FOMO
Input: 64x64 grayscale
Output: smoke_probability [0-1] + bounding box
FPS on OpenMV H7: ~30fps
RAM: ~80KB
Flash: ~120KB

训练数据:
  - 正样本: 各种烹饪烟雾场景 (煎、炒、炸、煮)
  - 负样本: 正常厨房环境、蒸汽(非油烟)
  - 数据增强: 旋转、亮度、对比度
```

### 3.2 人员检测 (FOMO + MobileNetV1 Backbone)

```
Model: FOMO (MobileNetV1 SSD)
Input: 96x96 RGB
Output: person_bbox [x, y, w, h, confidence]
FPS: ~15fps
RAM: ~200KB
Flash: ~300KB

特殊逻辑:
  - 每5帧检测一次 (降低功耗)
  - 连续N帧无人 → 触发待机倒计时
```

### 3.3 手势识别 (MobileNetV1 Transfer Learning)

```
Model: MobileNetV1 0.25 (alpha=0.25)
Input: 96x96 RGB
Output: 6 classes [无手势, 上滑, 下滑, 左滑, 右滑, 悬停]
FPS: ~25fps
RAM: ~150KB
Flash: ~250KB

手势映射:
  上滑 → 档位+1
  下滑 → 档位-1
  左滑 → 模式切换 (手动/自动)
  右滑 → 开关机
  悬停2s → 进入手势模式
```

## 4. 模型部署流程

```
1. Python训练 (TensorFlow/Keras)
     │
2. 导出TFLite + int8量化
     │
3. STM32Cube.AI 导入 → 生成 model.c / model.h
     │
4. CMSIS-NN/DSP 后端优化
     │
5. 集成到 ai_engine.c
     │
6. 板上验证 (延迟/精度/功耗)
```

## 5. 端侧AI优势

| 对比维度 | 云端方案 | 端侧方案 (本项目) |
|----------|---------|-------------------|
| 推理延迟 | 200-500ms | <50ms (传感器融合) |
| 网络依赖 | 必须联网 | 完全离线 |
| 隐私保护 | 数据上传 | 数据本地处理 |
| 功耗 | WiFi持续连接 | 仅本地计算 |
| 成本 | 云服务费用 | 一次性部署 |
