# CubeMX配置手册 — 智能油烟机 (Smart Hood System)

> 对应 .ioc 文件: `smart-hood-miniH562.ioc`
> MCU: STM32H562VIT6 (LQFP-100) | 板卡: Mini H562 V1.0 | CubeMX ≥ 6.14

---

## 一、时钟树配置 (8MHz HSE → 250MHz)

### 1.1 时钟源选择

| 时钟源 | 状态 | 频率 | 用途 |
|--------|------|------|------|
| **HSE** | ✅ ON | 8 MHz | PLL输入 (板上8MHz晶振) |
| **LSE** | ✅ ON | 32.768 kHz | RTC + IWDG |
| HSI | ❌ OFF | 64 MHz | 不启用 |
| CSI | ❌ OFF | 4 MHz | 不启用 |
| HSI48 | ❌ OFF | 48 MHz | 不启用 |

### 1.2 PLL配置 (Wide VCO Range)

```
HSE 8MHz ──→ [/PLLM=2] ──→ 4MHz ──→ [×PLLN=125] ──→ 500MHz VCO
                                                          │
                    ┌─────────────────────────────────────┤
                    │           │            │             │
                [/PLLR=2]  [/PLLP=2]   [/PLLQ=4]    其他输出
                    │           │            │
               250MHz      250MHz      125MHz
               SYSCLK     APB Timers   ADC/USART/I2C
```

**关键参数一览：**

| 参数 | 值 | 解释 |
|------|-----|------|
| PLL Source | HSE | |
| PLLM | ÷2 | 8MHz÷2=4MHz入VCO (VCO输入1-16MHz) |
| PLLN | ×125 | 4MHz×125=500MHz VCO (Wide: 192-836MHz) |
| PLLR | ÷2 | 500÷2=**250MHz SYSCLK** |
| PLLP | ÷2 | 500÷2=250MHz APB Timer Clocks |
| PLLQ | ÷4 | 500÷4=125MHz Periph Clocks |
| VCO Range | Wide (192-836MHz) | STM32H562支持Wide VCO |

### 1.3 总线时钟分频

| 总线 | 分频 | 频率 | 挂载外设 |
|------|------|------|----------|
| **SYSCLK** | — | **250 MHz** | CPU Core |
| **HCLK** (AHB) | ÷1 | **250 MHz** | DMA, GPIO, ADC总线 |
| **APB1** | ÷2 | **125 MHz** | I2C1, USART3, TIM2-7 |
| **APB2** | ÷2 | **125 MHz** | USART1, SPI1 |
| **APB3** | ÷2 | **125 MHz** | — |
| **TIM1** (APB2) | ×2 (timer) | **250 MHz** | Motor PWM |
| **TIM2** (APB1) | ×2 (timer) | **250 MHz** | FreeRTOS timebase |
| **ADC** | ÷2 async | **125 MHz** | ADC1 clock |

> **APB Timer时钟倍频规则**：当APB Prescaler ≠ 1时，Timer Clock = APB Clock × 2。
> 所以APB2=125MHz÷2×2=250MHz，正确。

### 1.4 Flash等待周期

```
SYSCLK 250MHz → FLASH_LATENCY = 5 (200-250MHz范围)
```

---

## 二、引脚配置详情

### 2.1 I2C1 — 传感器总线

| 引脚 | 功能 | 模式 | AF | 备注 |
|------|------|------|-----|------|
| **PB6** | I2C1_SCL | Alternate Function Open Drain | AF4 | SHT40+SGP40+SSD1306 |
| **PB7** | I2C1_SDA | Alternate Function Open Drain | AF4 | |

```
Timing寄存器: 0x10707DBC
    PRESC=1, SCLDEL=7, SDADEL=7
    SCLH=188, SCLL=219
    → I2C Fast Mode 400kHz @125MHz APB
```

### 2.2 USART1 — OpenMV通信 (921600bps)

| 引脚 | 功能 | 模式 | AF |
|------|------|------|-----|
| **PA9** | USART1_TX | Alternate Function Push-Pull | AF7 |
| **PA10** | USART1_RX | Alternate Function Push-Pull | AF7 |

```
波特率计算: 125MHz / 16 / 921600 = 8.482
    BRR = 8.5 (DIV_Mantissa=8, DIV_Fraction=8)
    实际波特率 = 125MHz / 16 / 8.5 = 919118 bps
    误差 = (921600-919118)/921600 = 0.27% ✓ (< 3%)
```

### 2.3 USART3 — PMS7003 (9600bps)

| 引脚 | 功能 | 模式 | AF |
|------|------|------|-----|
| **PD8** | USART3_TX | Alternate Function Push-Pull | AF7 |
| **PD9** | USART3_RX | Alternate Function Push-Pull | AF7 |

```
BRR = 125MHz / 16 / 9600 = 813.8
    DIV_Mantissa=813, DIV_Fraction=13
    实际 = 125MHz / 16 / 813.8125 = 9600.037 bps
    误差 < 0.001% ✓
```

### 2.5 ADC1 — ACS712电流采样

| 引脚 | 通道 | 采样时间 | 分辨率 |
|------|------|----------|--------|
| **PC4** | ADC1_INP4 | 24.5 cycles | 12-bit |

```
ADC时钟: 125MHz (PLLQ÷2, async div2)
采样率: 125MHz / (24.5+12.5) ≈ 3.38 MSPS (单次转换)
实际100Hz采样: 软件触发 + DMA循环模式
```

### 2.6 TIM1 — 电机PWM (20kHz)

| 引脚 | 功能 | AF |
|------|------|-----|
| **PA8** | TIM1_CH1 | AF1 |

```
PWM频率计算:
    TIM1_CLK = 250MHz (APB2×2)
    PSC = 0 (不分频)
    ARR = 12499
    f_PWM = 250MHz / (0+1) / (12499+1) = 20,000 Hz ✓

占空比分辨率:
    ARR=12499 → 0.008% per step → 6档:
    Level 0: 0 (0%), Level 1: 3750 (30%)
    Level 2: 5625 (45%), Level 3: 7500 (60%)
    Level 4: 10000 (80%), Level 5: 12499 (100%)
```

### 2.7 TIM2 — FreeRTOS Timebase (1ms)

```
TIM2_CLK = 250MHz (APB1×2)
PSC = 249 → 250MHz/250 = 1MHz
ARR = 999 → 1MHz/1000 = 1kHz → 1ms tick
```

### 2.8 GPIO引脚

| 引脚 | 方向 | 上下拉 | 初始值 | 标签 | 用途 |
|------|------|--------|--------|------|------|
| PA0 | Input | Pull-Up | — | KEY0 | 模式切换按键 |
| PA1 | Input | Pull-Up | — | KEY1 | 档位+按键 |
| PA15 | Input | Pull-Down | — | WK_UP | 电源待机按键 |
| PA6 | Output PP | NoPull | LOW | WS2812B | 空气质量LED |
| PC5 | Output PP | NoPull | LOW | MOTOR_DIR | 电机方向 |
| PD7 | Output PP | NoPull | LOW | MOTOR_EN | 电机使能 |
| PE0 | Output PP | NoPull | LOW | BUZZER | 蜂鸣器 |
| PC13 | Output PP | NoPull | LOW | DEBUG_LED | 调试LED |
| PA13 | SWDIO | — | — | SWD | 调试器 |
| PA14 | SWCLK | — | — | SWC | 调试器 |

### 2.9 未使用引脚（悬空或硬件固定）

- PA2, PA3, PA4, PA5: 设为Unused (模拟输入高阻态)
- PA11(USB_DM), PA12(USB_DP): 默认状态
- PB0-PB5, PB8-PB9, PB12-PB15: Unused
- PC0-PC3, PC6-PC12: Unused
- PD0-PD6, PD10-PD15: Unused
- PE1-PE15 (除PE0): Unused

---

## 三、DMA (GPDMA1) 通道分配

STM32H562使用**GPDMA**（通用DMA），有16个独立通道，每个通道可配置任意请求。

| 通道 | 外设请求 | 方向 | 模式 | 数据宽度 | 优先级 | 用途 |
|------|----------|------|------|----------|--------|------|
| **CH0** | USART1_RX | P→M | Circular | Byte | **High** | OpenMV JPEG 图像接收 |
| **CH1** | USART3_RX | P→M | Circular | Byte | Low | PMS7003 传感器帧 |
| **CH2** | ADC1 | P→M | Circular | HalfWord | Medium | ACS712 电流连续采样 |

> **说明**: STM32H5的GPDMA使用`GPDMA1_REQUEST_xxx`请求号，非传统DMA Stream/Channel模式。
> 每个通道互相独立，无固定映射表。

---

## 四、FreeRTOS (CMSIS_V2) 配置

### 4.1 内核参数

| 参数 | 值 | 说明 |
|------|-----|------|
| Version | CMSIS_V2 | CMSIS-RTOS v2 API |
| CPU Clock | 250 MHz | |
| Tick Rate | 1000 Hz | 1ms tick |
| Max Priorities | 7 | 0-6 (0=idle, 6=max) |
| Minimal Stack | 128 words | 512 bytes min |
| Total Heap | 32768 bytes | 32KB |
| Max Task Name | 16 chars | |
| Preemption | Enabled | 抢占式调度 |
| Time Slicing | Enabled | 同优先级轮转 |
| Mutexes | Enabled | |
| Recursive Mutexes | Enabled | |
| Counting Semaphores | Enabled | |
| Task Notifications | Enabled (3 entries) | |

### 4.2 任务创建（在代码中手动创建）

`.ioc`文件只包含`defaultTask`占位，实际任务在`main.c`中按以下方式创建：

```c
// SensorTask: 传感器采集 1Hz
xTaskCreate(vSensorTask, "Sensor", 512*4, NULL, 4, &hSensorTask);

// MotorTask: 电机控制 100Hz
xTaskCreate(vMotorTask, "Motor", 256*4, NULL, 3, &hMotorTask);

// AiTask: AI推理 1Hz
xTaskCreate(vAiTask, "AI", 2048*4, NULL, 3, &hAiTask);

// CommTask: 通信处理 事件驱动
xTaskCreate(vCommTask, "Comm", 512*4, NULL, 2, &hCommTask);

// SystemTask: 状态机 10Hz
xTaskCreate(vSystemTask, "System", 256*4, NULL, 1, &hSystemTask);
```

### 4.3 NVIC中断优先级

```
优先级分组: NVIC_PRIORITYGROUP_4 (4-bit preemption, 0-bit sub)

中断优先级分配 (0=最高, 15=最低):
    0:  HardFault, NMI, MemManage, BusFault, UsageFault
    3:  ADC1_IRQn (电流采样)
    4:  USART1_IRQn (OpenMV通信)
    5:  I2C1_EV_IRQn, I2C1_ER_IRQn (传感器I2C)
    6:  TIM1_UP_IRQn (PWM更新)
    7:  USART3_IRQn (PMS7003)
    15: SysTick_IRQn, PendSV_IRQn (FreeRTOS内核)
```

**重要**: FreeRTOS 管理的中断优先级范围是5-15（`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5`），0-4不应调用FreeRTOS API。

---

## 五、IWDG 独立看门狗

| 参数 | 值 | 说明 |
|------|-----|------|
| Prescaler | ÷64 | LSI=32kHz → 32k/64=500Hz |
| Reload | 4095 | 4095/500=8.19s 超时 |
| Window | 4095 | 无窗口限制 |

喂狗需在 < 8秒内调用 `HAL_IWDG_Refresh(&hiwdg)`。主循环10ms周期，安全余量充足。

---

## 六、在CubeMX中打开.ioc文件

1. 启动 STM32CubeMX ≥ 6.14
2. File → Load Project → 选择 `smart-hood-miniH562.ioc`
3. 检查 Pinout 视图，确认所有引脚标记正确
4. 检查 Clock Configuration 选项卡:
   - HSE = 8MHz
   - PLL = ON, /2 ×125 /2 → 250MHz SYSCLK
   - HCLK=250, APB1=125, APB2=125, APB3=125
5. Project Manager → Code Generator:
   - ✅ Copy only the necessary library files
   - ✅ Generate peripheral initialization as a pair of .c/.h files
   - ✅ Keep User Code when re-generating
6. 点击 **GENERATE CODE** 生成STM32CubeIDE工程

---

## 七、生成代码后需手动适配的固件改动

### 7.1 `config.h` (覆盖自动生成的MX常量)

```c
// ACS712参数修正 (5V供电+1:2分压)
#undef  HALL_SENSITIVITY
#define HALL_SENSITIVITY   0.0925f   // 分压后等效值
#undef  HALL_OFFSET_V
#define HALL_OFFSET_V      1.25f     // 5V供电零电流分压后

// 按键引脚修正
#define KEY0_PIN            GPIO_PIN_0   // PA0
#define KEY0_PORT           GPIOA
#define KEY1_PIN            GPIO_PIN_1   // PA1
#define KEY1_PORT           GPIOA
#define WKUP_PIN            GPIO_PIN_15  // PA15
#define WKUP_PORT           GPIOA

// 电机引脚修正
#define MOTOR_EN_PIN        GPIO_PIN_7   // PD7
#define MOTOR_EN_PORT       GPIOD
#define MOTOR_DIR_PIN       GPIO_PIN_5   // PC5
#define MOTOR_DIR_PORT      GPIOC
```

### 7.2 `main.c` (关键初始化顺序)

```c
// 正确的初始化顺序:
SystemClock_Config();   // 1. 时钟
MX_GPIO_Init();         // 2. GPIO
MX_GPDMA1_Init();       // 3. DMA
MX_I2C1_Init();         // 4. I2C (传感器通信)
MX_USART1_Init();       // 5. USART1 (OpenMV)
MX_USART3_Init();       // 6. USART3 (PMS7003)
MX_ADC1_Init();         // 8. ADC (ACS712)
MX_TIM1_Init();         // 9. TIM1 (PWM)
MX_IWDG1_Init();        // 10. 看门狗
MX_FREERTOS_Init();     // 11. FreeRTOS (CMSIS_V2)
// ... 任务创建
```

### 7.3 `hall.c` (ADC值→电流转换)

```c
float Hall_ADCToCurrent(uint16_t adc_value) {
    // ADC reading → voltage at ADC pin (0-3.3V)
    float vadc = ((float)adc_value / 4095.0f) * 3.3f;
    // 1:2 divider → actual ACS712 output voltage
    float vactual = vadc * 2.0f;
    // 5V supply, zero-current = 2.5V, sensitivity = 185mV/A
    float current = (vactual - 2.5f) / 0.185f;
    return current;
}
```

### 7.4 `motor_control.c` (TIM句柄)

```c
// 所有 HAL_TIM_PWM_xxx 和 __HAL_TIM_SET_COMPARE 的实例
// 从 htim2 改为 htim1
extern TIM_HandleTypeDef htim1;
// ...
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty);
```

---

> **按 `CubeMX配置步骤.md` 配置完成后，Generate Code即可获得匹配的HAL初始化代码。**
