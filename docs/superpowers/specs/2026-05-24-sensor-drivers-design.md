# 传感器驱动与融合层设计

> 版本：v1.0 | 日期：2026-05-24 | 状态：已审批
> MCU：STM32H562VIT6 | 平台：Mini H562 V1 | RTOS：FreeRTOS CMSIS_V2

---

## 一、设计目标

为智能油烟机项目开发 4 个传感器驱动（SHT40/SGP40/PMS7003/ACS712）及传感器融合层，运行在 FreeRTOS SensorTask 中，周期 1Hz。

---

## 二、文件布局

```
project/Core/
├── Inc/
│   ├── config.h              # 全局配置、传感器数据结构
│   ├── drivers/
│   │   ├── sht40.h
│   │   ├── sgp40.h
│   │   ├── pms7003.h
│   │   └── hall.h
│   └── app/
│       └── sensor_hub.h      # 融合层接口
├── Src/
│   ├── drivers/
│   │   ├── sht40.c
│   │   ├── sgp40.c
│   │   ├── pms7003.c
│   │   └── hall.c
│   ├── app/
│   │   └── sensor_hub.c      # 融合层实现
│   └── sensirion/            # Sensirion 官方库 (7文件，不改动原逻辑)
│       ├── sensirion_common.c/h
│       ├── sensirion_i2c.c/h
│       ├── sensirion_i2c_hal.c/h  ← 适配 STM32 HAL I2C
│       ├── sensirion_config.h
│       └── sgp40_i2c.c/h
```

---

## 三、传感器驱动接口

### 3.1 SHT40 — 温湿度 (I2C1, 0x44)

```
SHT40_Init()        → 复位传感器，验证 I2C 通信
SHT40_Read(*t, *h)  → 触发高精度测量 (0xFD)，等 8.3ms，读 6 字节，CRC8 校验
SHT40_GetSerial()   → 读 48-bit 序列号（可选）
```

- 依赖 hi2c1 (HAL handle)
- 不依赖 RTOS，内部用 HAL_Delay
- CRC8 多项式 0x31，初始值 0xFF
- 公式：t_degC = -45 + 175 * (t_ticks / 65535)，rh_pRH = -6 + 125 * (rh_ticks / 65535)

### 3.2 SGP40 — VOC 空气质量 (I2C1, 0x59)

```
SGP40_Init()                → 自检 + 读序列号
SGP40_MeasureRaw(t, h, *raw)→ 单次 VOC 测量，温湿度补偿
SGP40_HeaterOff()           → 关加热板
```

- 基于 Sensirion 官方嵌入式驱动库 (`embedded-i2c-sgp40-master`)
- `sensirion_i2c_hal.c` 的 read/write/sleep 三个函数适配 STM32 HAL I2C
- 其他 sensirion 文件不改动
- 温湿度 ticks 转换在驱动内部完成：`t_ticks = (t_c + 45) * 65535 / 175`，`h_ticks = rh_pct * 65535 / 100`

### 3.3 PMS7003 — PM2.5 粉尘 (USART3, 9600bps)

```
PMS7003_Init()          → 启动 DMA Circular 接收
PMS7003_GetData(*data)  → 获取最新完整帧数据
PMS7003_IsDataReady()   → 是否有新帧就绪
```

- GPDMA1_CH1 Circular 模式，32 字节循环缓冲区
- USART3 IDLE 中断触发帧解析状态机
- 帧头 0x42 0x4D，数据 26 字节 + 校验 2 字节
- 中断只写缓冲区+置标志，任务上下文读取
- 解析结果存入 pms7003_data_t (12个数据字段)

### 3.4 ACS712 — 电流检测 (ADC1_INP4, PC4)

```
Hall_Init()              → 启动 DMA ADC 采样
Hall_GetCurrent()        → 获取滑动窗口平均电流 (A)
Hall_GetInstantCurrent() → 获取瞬时电流 (A)
Hall_SetWindowSize(n)    → 设置滑动窗口点数
Hall_IsOvercurrent(thr)  → 过流检测
```

- GPDMA1_CH2 Circular 模式，256 halfwords 循环缓冲区
- 滑动窗口默认 64 采样点
- 转换：V_adc → V_actual (×2 分压还原) → I = (V_actual - 2.5) / 0.185
- MotorTask (100Hz) 可调用快速过流检测

---

## 四、融合层 (sensor_hub)

### 数据结构

```c
typedef struct {
    float    temperature;     // SHT40: °C
    float    humidity;        // SHT40: %RH
    uint16_t voc_raw;         // SGP40: raw ticks
    uint16_t voc_index;       // SGP40: 0-500 指数
    uint16_t pm2_5;           // PMS7003: μg/m³
    uint16_t pm10;            // PMS7003: μg/m³
    float    current;         // ACS712: A
    float    power;           // 功率 W
    uint32_t last_update;     // FreeRTOS tick
} sensor_data_t;
```

### 采集顺序

1. SHT40_Read — 先读温湿度
2. SGP40_MeasureRaw — 后读 VOC（依赖温湿度补偿），SHT40 失败则用默认值 25°C/50%RH
3. PMS7003_GetData — 独立，取最新帧
4. Hall_GetCurrent — 独立，取滑动平均电流

### 接口

```c
int8_t SensorHub_Init(void);             // 依次初始化 4 个传感器
void   SensorHub_Update(sensor_data_t *); // 执行一次完整采集
```

### 线程安全

- SensorTask (优先级 4, 1Hz) 独占调用 SensorHub_Update
- MotorTask (优先级 3, 100Hz) 调用 Hall_GetInstantCurrent 做快速过流检测
- Hall 的 ADC DMA 缓冲区用临界区保护（关中断几 μs），避免 SensorTask 和 MotorTask 竞争

---

## 五、FreeRTOS 集成

| 任务 | 优先级 | 周期 | 职责 |
|------|--------|------|------|
| SensorTask | 4 | 1s | SensorHub_Update → 更新全局 sensor_data_t |
| MotorTask | 3 | 10ms | Hall_GetInstantCurrent 过流检测 + PWM 斜坡 |
| AiTask | 3 | 1s | 读取 sensor_data_t，AI 推理 |
| CommTask | 2 | 事件驱动 | OpenMV 通信 + Debug 输出 |
| SystemTask | 1 | 100ms | 状态机 + 按键扫描 |

---

## 六、不做什么

- 传感器自校准算法（ACS712 零点漂移补偿）—— 留到下一轮
- SGP40 VOC Index 精确计算（需 Sensirion VOC Algorithm 库）—— 当前用简化的 raw→0-500 线性映射
- 传感器故障诊断 —— 只做基本返回值检查
- 低功耗管理 —— 传感器持续全速运行
