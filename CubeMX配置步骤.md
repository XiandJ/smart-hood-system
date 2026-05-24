# CubeMX 手动配置步骤 — 智能油烟机

> 请关闭之前打开 `.ioc` 的 CubeMX 窗口，然后按本文档**从头新建工程**。
> 每个步骤按CubeMX界面的选项卡顺序排列，逐步操作即可。

---

## 步骤0：新建工程

1. 打开 STM32CubeMX → **File → New Project**
2. 在 "MCU/MPU Selector" 选项卡中：
   - Series: **STM32H5**
   - Lines: **STM32H562/STM32H563**
   - 选择: **STM32H562VITx** (LQFP100)
3. 点击 **Start Project**

---

## 步骤1：Pinout → 引脚配置

在 Pinout 视图中，逐个设置以下引脚。**左键单击引脚 → 从弹出菜单选择功能**。

### 1.1 系统引脚（保持默认）

| 引脚 | 功能 | 说明 |
|------|------|------|
| PA13 | SYS_SWDIO | 调试器 |
| PA14 | SYS_SWCLK | 调试器 |
| PH0 | RCC_OSC_IN | 8MHz HSE |
| PH1 | RCC_OSC_OUT | 8MHz HSE |
| PC14 | RCC_OSC32_IN | 32.768KHz LSE |
| PC15 | RCC_OSC32_OUT | 32.768KHz LSE |

### 1.2 I2C1 传感器总线

| 引脚 | 左键菜单选择 |
|------|-------------|
| **PB6** | `I2C1_SCL` |
| **PB7** | `I2C1_SDA` |

### 1.3 USART1 — OpenMV (921600bps)

| 引脚 | 左键菜单选择 |
|------|-------------|
| **PA9** | `USART1_TX` |
| **PA10** | `USART1_RX` |

### 1.4 USART3 — PMS7003 (9600bps)

> PD8/PD9 的 AF7 可复用 USART3。PMS7003 外设不用改（仍是huart3）。

| 引脚 | 左键菜单选择 | 排针位置 |
|------|-------------|----------|
| **PD8** | `USART3_TX` | P1 Pin17 |
| **PD9** | `USART3_RX` | P1 Pin18 |

### 1.5 USART2 — 不使用

USART2 不配置。Debug 输出走 SSD1306 OLED + ESP32 WiFi。

### 1.6 ADC1 — ACS712 电流

| 引脚 | 左键菜单选择 |
|------|-------------|
| **PC4** | `ADC1_INP4` |

### 1.7 TIM1 — 电机PWM

| 引脚 | 左键菜单选择 |
|------|-------------|
| **PA8** | `TIM1_CH1` |

### 1.8 GPIO输出

左键单击引脚 → 选择 `GPIO_Output`，然后在右键菜单或底部配置面板设置参数：

| 引脚 | 标签 | GPIO output level | GPIO mode | GPIO Pull-up/Pull-down | Max speed |
|------|------|-------------------|------------|------------------------|-----------|
| **PA6** | WS2812B | Low | Output Push Pull | No pull-up/down | High |
| **PC5** | MOTOR_DIR | Low | Output Push Pull | No pull-up/down | Low |
| **PD7** | MOTOR_EN | Low | Output Push Pull | No pull-up/down | Low |
| **PE0** | BUZZER | Low | Output Push Pull | No pull-up/down | Low |
| **PC13** | DEBUG_LED | Low | Output Push Pull | No pull-up/down | Low |

### 1.9 Debug 输出方案

Debug 不占用独立UART，改用以下方式：
- 传感器数据 → **SSD1306 OLED** (I2C1, 0x3C) 实时显示
- 完整日志 → **ESP32 WiFi桥** → Python上位机 (已完成的ESP32固件)
- 紧急调试 → 板载 **PC13 LED** 闪烁故障码

### 1.10 GPIO输入（板上按键）

| 引脚 | 标签 | GPIO mode | GPIO Pull-up/Pull-down |
|------|------|-----------|------------------------|
| **PA0** | KEY0 | Input mode | Pull-up |
| **PA1** | KEY1 | Input mode | Pull-up |
| **PA15** | WK_UP | Input mode | Pull-down |

### 1.10 未使用引脚处理

选中以下所有未使用引脚（Ctrl+点击多选）→ 右键 → **Pin Reservation** → **Unused**：
PA2, PA3, PA4, PA5, PA7, PA11, PA12, PB0, PB1, PB2, PB3, PB4, PB5, PB8, PB9, PB12, PB13, PB14, PB15, PC0, PC1, PC2, PC3, PC6, PC7, PC8, PC9, PC10, PC11, PC12, PD0-PD6, PD10-PD15, PE1-PE15(除PE0)

---

## 步骤2：Pinout → System Core

左侧 **Categories** → **System Core**，逐项配置：

### 2.1 GPIO

确认步骤1中配置的引脚参数正确。

在下方列表中找到 PA6/PC5/PD7/PE0/PC13（Output引脚）：
- GPIO output level = Low
- GPIO mode = Output Push Pull
- User Label = 上述标签名

找到 PA0/PA1/PA15（Input引脚）：
- PA0, PA1: GPIO mode = Input mode, Pull-up
- PA15: GPIO mode = Input mode, Pull-down

### 2.2 NVIC

切换到 **NVIC** 选项卡，勾选启用以下中断：

| 中断线 | Enabled | Preemption |
|--------|---------|------------|
| ADC1 Global interrupt | ✅ | 5 |
| I2C1 Event interrupt | ✅ | 5 |
| I2C1 Error interrupt | ✅ | 5 |
| TIM1 Update interrupt | ✅ | 6 |
| USART1 Global interrupt | ✅ | 5 |
| USART3 Global interrupt | ✅ | 7 |

> 底部 **Priority Group** = **4 bits for pre-emption priority**

### 2.3 RCC

| 选项 | 值 |
|------|-----|
| High Speed Clock (HSE) | **Crystal/Ceramic Resonator** |
| Low Speed Clock (LSE) | **Crystal/Ceramic Resonator** |

### 2.4 SYS (System)

| 选项 | 值 |
|------|-----|
| Debug | **Serial Wire** |
| Timebase Source | **TIM2** |

### 2.5 IWDG1 (Independent Watchdog)

勾选 **Activated**

| 参数 | 值 |
|------|-----|
| Prescaler | **64** |
| Window | **4095** |
| Down-counter reload value | **4095** |

---

## 步骤3：Analog → ADC1

左侧 **Categories** → **Analog** → **ADC1**，勾选 **IN4 (Single-ended)**

| 参数 | 值 |
|------|-----|
| Clock Prescaler | **Asynchronous clock mode divided by 2** |
| Resolution | **12 bits (15.5 ADC cycles)** |
| Data Alignment | **Right alignment** |
| Scan Conversion Mode | **Disabled** |
| Continuous Conversion Mode | **Disabled** |
| External Trigger Conversion Source | **Software trigger** |
| Number of Conversions | **1** |
| Rank 1 Channel | **Channel 4** |
| Rank 1 Sampling Time | **24.5 cycles** |

---

## 步骤4：Timers → TIM1 & TIM2

### 4.1 TIM1 (Motor PWM)

左侧 **Categories** → **Timers** → **TIM1**

| 参数 | 值 |
|------|-----|
| Clock Source | **Internal Clock** |
| Channel 1 | **PWM Generation CH1** |

下方 Configuration 面板：

| 参数 | 值 |
|------|-----|
| Prescaler (PSC) | **0** |
| Counter Mode | **Up** |
| Counter Period (ARR) | **12499** |
| Auto-reload preload | **Enable** |
| Internal Clock Division | **No Division** |
| Repetition Counter | **0** |
| PWM Mode (CH1) | **Mode 1** |
| Pulse (CH1) | **0** (初始占空比0%) |
| CH Polarity | **High** |

### 4.2 TIM2 (FreeRTOS Timebase)

左侧 **Categories** → **Timers** → **TIM2**

| 参数 | 值 |
|------|-----|
| Clock Source | **Internal Clock** |
| Prescaler (PSC) | **249** |
| Counter Period (ARR) | **999** |

不需要任何Channel配置，TIM2只用作时基。

---

## 步骤5：Connectivity → USART1, USART3

### 5.1 USART1 — OpenMV (921600bps)

左侧 **Categories** → **Connectivity** → **USART1**

| 参数 | 值 |
|------|-----|
| Mode | **Asynchronous** |
| Baud Rate | **921600** |
| Word Length | **8 Bits (including Parity)** |
| Parity | **None** |
| Stop Bits | **1** |
| Over Sampling | **16 Samples** |

### 5.2 USART3 — PMS7003 (9600bps)

| 参数 | 值 |
|------|-----|
| Mode | **Asynchronous** |
| Baud Rate | **9600** |
| Word Length | **8 Bits** |
| Parity | **None** |
| Stop Bits | **1** |

### 5.3 USART2 — 跳过

不配置。（Debug走SSD1306+ESP32）

---

## 步骤6：Connectivity → I2C1

左侧 **Categories** → **Connectivity** → **I2C1**

| 参数 | 值 |
|------|-----|
| I2C Speed Mode | **Fast Mode** |
| I2C Speed Frequency (KHz) | **400** |
| Rise Time (ns) | **100** |
| Fall Time (ns) | **10** |

> CubeMX会基于APB1频率（125MHz）自动计算Timing寄存器值。无需手动填。

---

## 步骤7：System Core → DMA (GPDMA1)

左侧 **Categories** → **System Core** → **DMA/GPDMA**

点击 **Add** 添加以下 **3个通道**：

### CH0: USART1_RX (OpenMV图像)

| 参数 | 值 |
|------|-----|
| DMA Request | **USART1_RX** |
| Direction | **Peripheral to Memory** |
| Priority | **High** |
| Mode | **Circular** |
| Increment Address (Memory) | ✅ |
| Increment Address (Peripheral) | ❌ |
| Data Width (Both) | **Byte** |

### CH1: USART3_RX (PMS7003数据)

| 参数 | 值 |
|------|-----|
| DMA Request | **USART3_RX** |
| Direction | **Peripheral to Memory** |
| Priority | **Low** |
| Mode | **Circular** |
| Increment Address (Memory) | ✅ |
| Increment Address (Peripheral) | ❌ |
| Data Width (Both) | **Byte** |

### CH2: ADC1 (ACS712电流)

| 参数 | 值 |
|------|-----|
| DMA Request | **ADC1** |
| Direction | **Peripheral to Memory** |
| Priority | **Medium** |
| Mode | **Circular** |
| Increment Address (Memory) | ✅ |
| Increment Address (Peripheral) | ❌ |
| Data Width (Both) | **Half Word** |

---

## 步骤8：Middleware → FREERTOS

左侧 **Categories** → **Middleware and Software Packs** → **FREERTOS**

### 8.1 选择接口

Interface: **CMSIS_V2**

### 8.2 Configuration 选项卡

在 Configuration 面板中（不是 Config parameters 小按钮），直接修改左侧列表中的值：

| 参数 | 值 |
|------|-----|
| **USE_PREEMPTION** | Enabled |
| **TICK_RATE_HZ** | 1000 |
| **MAX_PRIORITIES** | 7 |
| **MINIMAL_STACK_SIZE** | 128 |
| **TOTAL_HEAP_SIZE** | 32768 |
| **MAX_TASK_NAME_LEN** | 16 |
| **USE_16_BIT_TICKS** | Disabled |
| **IDLE_SHOULD_YIELD** | Enabled |
| **USE_MUTEXES** | Enabled |
| **USE_RECURSIVE_MUTEXES** | Enabled |
| **USE_COUNTING_SEMAPHORES** | Enabled |
| **USE_TIME_SLICING** | Enabled |
| **USE_NEWLIB_REENTRANT** | Enabled |
| **USE_TASK_NOTIFICATIONS** | Enabled |
| **TASK_NOTIFICATION_ARRAY_ENTRIES** | 3 |

### 8.3 Include Parameters 选项卡

将以下全部设为 **Enabled**：

- `vTaskPrioritySet`
- `uxTaskPriorityGet`
- `vTaskDelete`
- `vTaskSuspend`
- `xResumeFromISR`
- `vTaskDelayUntil`
- `vTaskDelay`
- `xTaskGetSchedulerState`
- `xTaskGetCurrentTaskHandle`
- `uxTaskGetStackHighWaterMark`
- `eTaskGetState`
- `xEventGroupSetBitFromISR`
- `xTimerPendFunctionCall`
- `xTaskAbortDelay`
- `xTaskGetHandle`
- `xTaskResumeFromISR`

### 8.4 Tasks 选项卡

保留默认的 `defaultTask`（Entry=StartDefaultTask, Priority=osPriorityNormal, Stack=512）

实际任务将在代码中手动创建。这里只保留占位。

---

## 步骤9：Clock Configuration

切换到 **Clock Configuration** 选项卡。

### 9.1 输入频率设置

在顶部输入框填写：
- **HSE**: `8` MHz (已自动检测为8MHz)
- **LSE**: `32.768` KHz (已自动)

### 9.2 PLL Source Mux

选择: **HSE** (非 HSI)

### 9.3 PLL 参数

在 PLL 块中设置（从顶向下）：

| 参数 | 值 | 下拉选择或直接填写 |
|------|-----|-------------------|
| PLLM | **/2** | |
| PLLN | **×125** | |
| PLL VCO Range | **Wide** | ← 重要: 选Wide (192-836MHz) |
| PLLR | **/2** | → SYSCLK |
| PLLP | **/2** | → Peripherals |
| PLLQ | **/4** | → ADC/USART/I2C |

应看到：
```
VCO = 500 MHz
PLLR → SYSCLK = 250 MHz ✓
```

### 9.4 总线分频

在 System Clock Mux 块设置：

| 总线 | 分频 |
|------|------|
| SYSCLK → HCLK | **÷1** (250 MHz) |
| HCLK → APB1 | **÷2** (125 MHz) |
| HCLK → APB2 | **÷2** (125 MHz) |
| HCLK → APB3 | **÷2** (125 MHz) |

各外设时钟应自动显示：
- TIM1: 250 MHz
- TIM2: 250 MHz
- USART1: 125 MHz
- USART3: 125 MHz
- USART3: 125 MHz

---

## 步骤10：Project Manager

### 10.1 Project 选项卡

| 设置 | 值 |
|------|-----|
| Project Name | **smart-hood-miniH562** |
| Project Location | 选择你的工作目录 |
| Application Structure | **Advanced** (默认) |
| Toolchain / IDE | **STM32CubeIDE** |

### 10.2 Code Generator 选项卡

| 设置 | 值 |
|------|-----|
| Copy only the necessary library files | ✅ |
| Generate peripheral initialization as a pair of .c/.h files | ✅ |
| Keep User Code when re-generating | ✅ |
| Delete previously generated files when not re-generated | ❌ |

---

## 步骤11：导出与验证

1. 点击工具栏 **GENERATE CODE** 按钮
2. 等待代码生成完成 → **Open Project** 在 STM32CubeIDE 中打开
3. 验证以下生成的关键代码：

### 验证清单

- [ ] `main.c` 中包含 `MX_I2C1_Init()`, `MX_USART1_Init()`, `MX_USART3_Init()`
- [ ] `main.c` 中包含 `MX_ADC1_Init()`, `MX_TIM1_Init()`, `MX_TIM2_Init()`
- [ ] `main.c` 中包含 `MX_DMA_Init()` (GPDMA1, 3通道)
- [ ] `main.c` 中包含 `MX_FREERTOS_Init()`
- [ ] `stm32h5xx_hal_msp.c` 中引脚初始化正确
- [ ] `i2c1.c` 中 `hi2c1.Init.Timing` 非零
- [ ] `tim1.c` 中 `htim1.Init.Period = 12499`, `Prescaler = 0`
- [ ] `adc1.c` 中 `sConfig.Channel = ADC_CHANNEL_4`
- [ ] `usart1.c` 中 `huart1.Init.BaudRate = 921600`
- [ ] `usart3.c` 中 `huart3.Init.BaudRate = 9600`

---

## 步骤12：生成代码后 → 适配固件

### 快速适配checklist

- [ ] `config.h`: `HALL_SENSITIVITY` → `0.0925f`, `HALL_OFFSET_V` → `1.25f`
- [ ] `config.h`: 按键引脚宏 → PA0/PA1/PA15
- [ ] `config.h`: 移除 `DEBUG_ENABLE` 或改为0（Debug改用OLED/ESP32）
- [ ] `main.c` `SystemClock_Config()`: HSE=8MHz (非固件默认25MHz)
- [ ] `pms7003.c`: **不动**（PD8/PD9用USART3，外设仍是huart3）
- [ ] `motor_control.c`: `htim2` → `htim1`
- [ ] `hall.c`: `ADC_CHANNEL_1` → `ADC_CHANNEL_4`, 分压公式修正
- [ ] `sensor_hub.c` `SensorHub_PrintDebug()`: 改用OLED输出或删除
- [ ] FreeRTOS 保持不变，CMSIS_V2 API 无需改动

---

> 以上步骤在CubeMX中操作完成后，工程即可用于编译。若有任何步骤配置与描述不符，随时反馈。
