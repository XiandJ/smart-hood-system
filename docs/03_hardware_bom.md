# 智能抽油烟机系统 - 硬件BOM与接口定义

## 1. 物料清单 (BOM)

| 序号 | 元件 | 型号 | 数量 | 接口 | 备注 |
|------|------|------|------|------|------|
| 1 | 主控MCU | STM32H562VIT6 | 1 | - | LQFP-100, Cortex-M33 |
| 2 | 视觉模组 | OpenMV Cam H7 Plus | 1 | UART | STM32H743VI, OV5640 |
| 3 | 温湿度传感器 | SHT40-AD1B-R2 | 1 | I2C (0x44) | ±0.2°C, ±1.8%RH |
| 4 | VOC传感器 | SGP40-D-R4 | 1 | I2C (0x59) | 0-500 VOC Index |
| 5 | 颗粒物传感器 | PMS7003 | 1 | UART (9600bps) | PM1.0/2.5/10 |
| 6 | 霍尔电流传感器 | ACS712-10A | 1 | ADC | 0-10A, 185mV/A |
| 7 | 无刷直流电机 | BLDC 24V/60W | 1 | PWM | 20kHz PWM |
| 8 | MOS驱动 | IR2104 + IRF540 | 1 | GPIO | 半桥驱动 |
| 9 | OLED显示屏 | SSD1306 0.96" | 1 | I2C (0x3C) | 128x64 |
| 10 | LED指示灯 | WS2812B x4 | 4 | GPIO | 空气质量指示 |
| 11 | 按键 | 轻触开关 | 3 | GPIO | 电源/模式/档位 |
| 12 | WiFi/BLE模块 | ESP32-C3-MINI-1 | 1 | UART2 | 可选扩展 |

## 2. STM32H562VIT6 引脚分配

```
Pin     Function    Mode        Connected To
─────────────────────────────────────────────
PA0     TIM2_CH1    PWM Output  Motor PWM (IR2104 IN)
PA1     ADC1_IN1    Analog In   ACS712 VOUT
PA2     GPIO_OUT    Push-Pull   Motor Enable
PA3     GPIO_IN     Input PU    按键: 电源
PA4     GPIO_IN     Input PU    按键: 模式
PA5     GPIO_IN     Input PU    按键: 档位+
PA6     GPIO_OUT    Push-Pull   WS2812B Data
PA9     USART1_TX   AF PP       OpenMV RX
PA10    USART1_RX   AF PP       OpenMV TX
PA13    SWDIO       -           Debugger
PA14    SWCLK       -           Debugger
PB6     I2C1_SCL    AF OD       SHT40 SCL, SGP40 SCL, SSD1306 SCL
PB7     I2C1_SDA    AF OD       SHT40 SDA, SGP40 SDA, SSD1306 SDA
PB10    USART3_TX   AF PP       PMS7003 RX
PB11    USART3_RX   AF PP       PMS7003 TX
PD8     USART2_TX   AF PP       ESP32-C3 RX (预留)
PD9     USART2_RX   AF PP       ESP32-C3 TX (预留)
PC13    GPIO_OUT    Push-Pull   LED (板载调试)
```

## 3. I2C总线设备地址

| 设备 | 7-bit地址 | 最大速率 |
|------|-----------|----------|
| SHT40 | 0x44 | 400 kHz (Fast Mode) |
| SGP40 | 0x59 | 400 kHz |
| SSD1306 | 0x3C | 400 kHz |

## 4. 电源树

```
24V DC IN ──→ LM2596 (5V/3A) ──→ AMS1117-3.3 (3.3V/1A)
            │                   │
            ├── Motor Driver    ├── STM32H562
            │                   ├── OpenMV (5V)
            │                   ├── PMS7003 (5V)
            │                   └── Sensors (3.3V)
            └── ESP32-C3 (3.3V)
```
