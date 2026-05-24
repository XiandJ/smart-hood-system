# 传感器驱动与融合层 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为智能油烟机项目创建 4 个传感器驱动（SHT40/SGP40/PMS7003/ACS712）+ 融合层（sensor_hub），集成到 FreeRTOS SensorTask 中运行。

**Architecture:** 扁平驱动层，每个传感器一个 .c/.h 文件对。SGP40 复用 Sensirion 官方 I2C 库（适配 STM32 HAL）。融合层顺序采集：SHT40 → SGP40 → PMS7003 → ACS712，汇总到 sensor_data_t。

**Tech Stack:** STM32H562VIT6, STM32 HAL, FreeRTOS CMSIS_V2, Keil MDK-ARM

---

### Task 1: 创建目录结构 + config.h

**Files:**
- Create: `project/Core/Inc/config.h`
- Create: `project/Core/Inc/drivers/` (dir)
- Create: `project/Core/Inc/app/` (dir)
- Create: `project/Core/Src/drivers/` (dir)
- Create: `project/Core/Src/app/` (dir)
- Create: `project/Core/Src/sensirion/` (dir)

- [ ] **Step 1: 创建目录**

```bash
mkdir -p project/Core/Inc/drivers
mkdir -p project/Core/Inc/app
mkdir -p project/Core/Src/drivers
mkdir -p project/Core/Src/app
mkdir -p project/Core/Src/sensirion
```

- [ ] **Step 2: 编写 config.h**

> 写到 `project/Core/Inc/config.h`

```c
#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdint.h>

/* ====== 传感器状态 ====== */
typedef enum {
    SENSOR_OK = 0,
    SENSOR_TIMEOUT,
    SENSOR_CRC_ERROR,
    SENSOR_NODATA,
    SENSOR_ERROR
} sensor_status_t;

/* ====== 全局传感器数据 ====== */
typedef struct {
    float    temperature;
    float    humidity;
    uint16_t voc_raw;
    uint16_t voc_index;
    uint16_t pm2_5;
    uint16_t pm10;
    float    current;
    float    power;
    uint32_t last_update;
} sensor_data_t;

/* ====== ACS712 参数 (5V供电 + 1:2分压) ====== */
#define HALL_SENSITIVITY   0.185f
#define HALL_OFFSET_V_5V   2.5f
#define HALL_DIVIDER_RATIO 2.0f
#define HALL_ADC_VREF      3.3f
#define HALL_ADC_RES       4095.0f

/* ====== ADC 滑动窗口 ====== */
#define HALL_ADC_BUF_SIZE  256
#define HALL_WINDOW_SIZE   64

/* ====== I2C 地址 ====== */
#define SHT40_I2C_ADDR     0x44
#define SGP40_I2C_ADDR     0x59

/* ====== PMS7003 ====== */
#define PMS7003_FRAME_LEN  32

#endif /* __CONFIG_H */
```

- [ ] **Step 3: 验证编译**

在 Keil MDK 中打开工程，确认 `config.h` 无编译错误。

---

### Task 2: SHT40 温湿度驱动

**Files:**
- Create: `project/Core/Inc/drivers/sht40.h`
- Create: `project/Core/Src/drivers/sht40.c`

- [ ] **Step 1: 编写 sht40.h**

> 写到 `project/Core/Inc/drivers/sht40.h`

```c
#ifndef __SHT40_H
#define __SHT40_H

#include <stdint.h>

int8_t SHT40_Init(void);
int8_t SHT40_Read(float *temp_c, float *hum_pct);
int8_t SHT40_GetSerial(uint32_t *serial);

#endif /* __SHT40_H */
```

- [ ] **Step 2: 编写 sht40.c**

> 写到 `project/Core/Src/drivers/sht40.c`

```c
#include "drivers/sht40.h"
#include "config.h"
#include "i2c.h"
#include "main.h"

#define SHT40_CMD_MEASURE_HIGH  0xFD
#define SHT40_CMD_SERIAL        0x89
#define CRC8_POLY               0x31
#define CRC8_INIT               0xFF

static uint8_t sht40_crc8(const uint8_t *data, uint16_t len) {
    uint8_t crc = CRC8_INIT;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ CRC8_POLY) : (crc << 1);
        }
    }
    return crc;
}

int8_t SHT40_Init(void) {
    uint8_t cmd = 0x94; /* soft reset */
    if (HAL_I2C_Master_Transmit(&hi2c1, SHT40_I2C_ADDR << 1, &cmd, 1, 10) != HAL_OK)
        return -1;
    HAL_Delay(5);
    return 0;
}

int8_t SHT40_Read(float *temp_c, float *hum_pct) {
    uint8_t cmd = SHT40_CMD_MEASURE_HIGH;
    uint8_t buf[6];

    if (HAL_I2C_Master_Transmit(&hi2c1, SHT40_I2C_ADDR << 1, &cmd, 1, 10) != HAL_OK)
        return -1;

    HAL_Delay(10);

    if (HAL_I2C_Master_Receive(&hi2c1, SHT40_I2C_ADDR << 1, buf, 6, 20) != HAL_OK)
        return -2;

    if (sht40_crc8(buf, 2) != buf[2]) return -3;
    if (sht40_crc8(buf + 3, 2) != buf[5]) return -3;

    uint16_t t_ticks = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t rh_ticks = ((uint16_t)buf[3] << 8) | buf[4];

    *temp_c   = -45.0f + 175.0f * ((float)t_ticks / 65535.0f);
    *hum_pct  = -6.0f  + 125.0f * ((float)rh_ticks / 65535.0f);

    if (*hum_pct > 100.0f) *hum_pct = 100.0f;
    if (*hum_pct < 0.0f)   *hum_pct = 0.0f;

    return 0;
}

int8_t SHT40_GetSerial(uint32_t *serial) {
    uint8_t cmd[2] = {SHT40_CMD_SERIAL >> 8, SHT40_CMD_SERIAL & 0xFF};
    uint8_t buf[6];

    if (HAL_I2C_Master_Transmit(&hi2c1, SHT40_I2C_ADDR << 1, cmd, 2, 10) != HAL_OK)
        return -1;
    HAL_Delay(2);

    if (HAL_I2C_Master_Receive(&hi2c1, SHT40_I2C_ADDR << 1, buf, 6, 20) != HAL_OK)
        return -2;

    if (sht40_crc8(buf, 2) != buf[2]) return -3;
    if (sht40_crc8(buf + 3, 2) != buf[5]) return -3;

    *serial = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
            | ((uint32_t)buf[3] << 8)  | buf[4];
    return 0;
}
```

- [ ] **Step 3: 验证编译**

在 Keil MDK 中重新编译，确认 `sht40.c` 无编译错误和警告。

---

### Task 3: Sensirion 库集成 + SGP40 驱动封装

**Files:**
- Copy: `embedded-i2c-sgp40-master/sensirion_common.c` → `project/Core/Src/sensirion/sensirion_common.c`
- Copy: `embedded-i2c-sgp40-master/sensirion_common.h` → `project/Core/Inc/sensirion/sensirion_common.h`
- Copy: `embedded-i2c-sgp40-master/sensirion_i2c.c` → `project/Core/Src/sensirion/sensirion_i2c.c`
- Copy: `embedded-i2c-sgp40-master/sensirion_i2c.h` → `project/Core/Inc/sensirion/sensirion_i2c.h`
- Copy: `embedded-i2c-sgp40-master/sensirion_config.h` → `project/Core/Inc/sensirion/sensirion_config.h`
- Copy: `embedded-i2c-sgp40-master/sensirion_i2c_hal.h` → `project/Core/Inc/sensirion/sensirion_i2c_hal.h`
- Copy: `embedded-i2c-sgp40-master/sgp40_i2c.c` → `project/Core/Src/sensirion/sgp40_i2c.c`
- Copy: `embedded-i2c-sgp40-master/sgp40_i2c.h` → `project/Core/Inc/sensirion/sgp40_i2c.h`
- Modify: `project/Core/Inc/sensirion/sensirion_config.h` — 去掉 arduino 依赖
- Create: `project/Core/Src/sensirion/sensirion_i2c_hal.c` — STM32 HAL 适配实现
- Create: `project/Core/Inc/drivers/sgp40.h`
- Create: `project/Core/Src/drivers/sgp40.c`

- [ ] **Step 1: 复制 Sensirion 官方文件到项目中**

```bash
# 复制到 sensirion 目录
cp embedded-i2c-sgp40-master/sensirion_common.c project/Core/Src/sensirion/
cp embedded-i2c-sgp40-master/sensirion_common.h project/Core/Inc/sensirion/
cp embedded-i2c-sgp40-master/sensirion_i2c.c     project/Core/Src/sensirion/
cp embedded-i2c-sgp40-master/sensirion_i2c.h     project/Core/Inc/sensirion/
cp embedded-i2c-sgp40-master/sensirion_config.h  project/Core/Inc/sensirion/
cp embedded-i2c-sgp40-master/sensirion_i2c_hal.h project/Core/Inc/sensirion/
cp embedded-i2c-sgp40-master/sgp40_i2c.c         project/Core/Src/sensirion/
cp embedded-i2c-sgp40-master/sgp40_i2c.h         project/Core/Inc/sensirion/
```

- [ ] **Step 2: 修改 sensirion_config.h**

> 编辑 `project/Core/Inc/sensirion/sensirion_config.h`，把 `#include <stdlib.h>` 替换为 `#include <stdint.h>` + `NULL` 定义（STM32 裸机环境不需要 stdlib.h）

```c
#ifndef SENSIRION_CONFIG_H
#define SENSIRION_CONFIG_H

#include <stdint.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef __cplusplus
#if __STDC_VERSION__ >= 199901L
#include <stdbool.h>
#else
#ifndef bool
#define bool int
#define true 1
#define false 0
#endif
#endif
#endif

#endif /* SENSIRION_CONFIG_H */
```

- [ ] **Step 3: 编写 sensirion_i2c_hal.c — STM32 HAL 适配**

> 写到 `project/Core/Src/sensirion/sensirion_i2c_hal.c`

```c
#include "sensirion_i2c_hal.h"
#include "sensirion_common.h"
#include "sensirion_config.h"
#include "i2c.h"

int16_t sensirion_i2c_hal_select_bus(uint8_t bus_idx) {
    (void)bus_idx;
    return 0; /* 单总线，无需切换 */
}

void sensirion_i2c_hal_init(void) {
    /* I2C1 已在 MX_I2C1_Init() 中初始化 */
}

void sensirion_i2c_hal_free(void) {
    /* 无需释放 */
}

int8_t sensirion_i2c_hal_read(uint8_t address, uint8_t *data, uint16_t count) {
    HAL_StatusTypeDef ret =
        HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(address << 1), data, count, 100);
    return (ret == HAL_OK) ? 0 : -1;
}

int8_t sensirion_i2c_hal_write(uint8_t address, const uint8_t *data, uint16_t count) {
    HAL_StatusTypeDef ret =
        HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(address << 1), (uint8_t *)data, count, 100);
    return (ret == HAL_OK) ? 0 : -1;
}

void sensirion_i2c_hal_sleep_usec(uint32_t useconds) {
    uint32_t ms = (useconds + 999) / 1000;
    if (ms == 0) ms = 1;
    HAL_Delay(ms);
}
```

- [ ] **Step 4: 编写 sgp40.h — 封装接口**

> 写到 `project/Core/Inc/drivers/sgp40.h`

```c
#ifndef __SGP40_H
#define __SGP40_H

#include <stdint.h>

int8_t SGP40_Init(void);
int8_t SGP40_MeasureRaw(float temp_c, float hum_pct, uint16_t *sraw_voc);
int8_t SGP40_HeaterOff(void);

uint16_t SGP40_RawToVOCIndex(uint16_t sraw_voc);

#endif /* __SGP40_H */
```

- [ ] **Step 5: 编写 sgp40.c — 封装实现**

> 写到 `project/Core/Src/drivers/sgp40.c`

```c
#include "drivers/sgp40.h"
#include "config.h"
#include "sensirion/sgp40_i2c.h"

static uint8_t initialized = 0;

int8_t SGP40_Init(void) {
    uint16_t test_result;
    int16_t ret = sgp40_execute_self_test(&test_result);
    if (ret != 0) return -1;
    if (test_result != 0xD400) return -2;
    initialized = 1;
    return 0;
}

int8_t SGP40_MeasureRaw(float temp_c, float hum_pct, uint16_t *sraw_voc) {
    if (!initialized) return -1;

    if (hum_pct > 100.0f) hum_pct = 100.0f;
    if (hum_pct < 0.0f)   hum_pct = 0.0f;
    if (temp_c > 130.0f)  temp_c = 130.0f;
    if (temp_c < -45.0f)  temp_c = -45.0f;

    uint16_t rh_ticks  = (uint16_t)((hum_pct / 100.0f) * 65535.0f);
    uint16_t t_ticks   = (uint16_t)(((temp_c + 45.0f) / 175.0f) * 65535.0f);

    int16_t ret = sgp40_measure_raw_signal(rh_ticks, t_ticks, sraw_voc);
    return (ret == 0) ? 0 : -2;
}

int8_t SGP40_HeaterOff(void) {
    int16_t ret = sgp40_turn_heater_off();
    return (ret == 0) ? 0 : -1;
}

uint16_t SGP40_RawToVOCIndex(uint16_t sraw_voc) {
    /* 简化映射: raw ticks → 0-500 */
    if (sraw_voc < 10000) return 0;
    if (sraw_voc > 60000) return 500;
    return (uint16_t)(((float)(sraw_voc - 10000) / 50000.0f) * 500.0f);
}
```

- [ ] **Step 6: 创建 sensirion Inc 目录**

```bash
mkdir -p project/Core/Inc/sensirion
```

- [ ] **Step 7: 在 Keil 工程中添加源文件**

将以下 .c 文件添加到 Keil MDK-ARM 工程的 Source Group：
- `Core/Src/sensirion/sensirion_common.c`
- `Core/Src/sensirion/sensirion_i2c.c`
- `Core/Src/sensirion/sensirion_i2c_hal.c`
- `Core/Src/sensirion/sgp40_i2c.c`
- `Core/Src/drivers/sgp40.c`

将以下路径添加到 Include Paths：
- `Core/Inc/sensirion`

- [ ] **Step 8: 验证编译**

---

### Task 4: PMS7003 粉尘传感器驱动

**Files:**
- Create: `project/Core/Inc/drivers/pms7003.h`
- Create: `project/Core/Src/drivers/pms7003.c`

- [ ] **Step 1: 编写 pms7003.h**

> 写到 `project/Core/Inc/drivers/pms7003.h`

```c
#ifndef __PMS7003_H
#define __PMS7003_H

#include <stdint.h>

typedef struct {
    uint16_t pm1_0_cf1;
    uint16_t pm2_5_cf1;
    uint16_t pm10_cf1;
    uint16_t pm1_0_atm;
    uint16_t pm2_5_atm;
    uint16_t pm10_atm;
    uint16_t cnt_0_3um;
    uint16_t cnt_0_5um;
    uint16_t cnt_1_0um;
    uint16_t cnt_2_5um;
    uint16_t cnt_5_0um;
    uint16_t cnt_10_0um;
    uint8_t  version;
    uint8_t  error_code;
    uint8_t  checksum_ok;
} pms7003_data_t;

int8_t  PMS7003_Init(void);
int8_t  PMS7003_GetData(pms7003_data_t *data);
uint8_t PMS7003_IsDataReady(void);

#endif /* __PMS7003_H */
```

- [ ] **Step 2: 编写 pms7003.c**

> 写到 `project/Core/Src/drivers/pms7003.c`

```c
#include "drivers/pms7003.h"
#include "config.h"
#include "usart.h"

#define PMS_HEADER1      0x42
#define PMS_HEADER2      0x4D
#define PMS_DMA_BUF_SIZE 64

typedef enum {
    PMS_STATE_IDLE,
    PMS_STATE_HDR1,
    PMS_STATE_DATA
} pms_state_t;

static uint8_t  rx_dma_buf[PMS_DMA_BUF_SIZE];
static uint8_t  rx_frame[PMS7003_FRAME_LEN];
static volatile uint8_t  rx_pos;
static volatile pms_state_t state = PMS_STATE_IDLE;
static volatile uint16_t frame_len;

static pms7003_data_t current_data;
static volatile uint8_t  data_ready = 0;

static uint16_t pms_make_word(uint8_t hi, uint8_t lo) {
    return ((uint16_t)hi << 8) | lo;
}

static void pms_parse_byte(uint8_t byte) {
    switch (state) {
    case PMS_STATE_IDLE:
        if (byte == PMS_HEADER1) {
            rx_frame[0] = byte;
            rx_pos = 1;
            state = PMS_STATE_HDR1;
        }
        break;
    case PMS_STATE_HDR1:
        if (byte == PMS_HEADER2) {
            rx_frame[1] = byte;
            rx_pos = 2;
            state = PMS_STATE_DATA;
        } else {
            state = PMS_STATE_IDLE;
        }
        break;
    case PMS_STATE_DATA:
        rx_frame[rx_pos++] = byte;
        if (rx_pos >= 4) {
            frame_len = pms_make_word(rx_frame[2], rx_frame[3]);
        }
        if (rx_pos >= 4 + frame_len + 2) {
            /* 完整帧收到，校验 */
            uint16_t sum = 0;
            for (uint16_t i = 0; i < rx_pos - 2; i++) {
                sum += rx_frame[i];
            }
            uint16_t chk = pms_make_word(rx_frame[rx_pos - 2], rx_frame[rx_pos - 1]);
            if (sum == chk && frame_len >= 28) {
                current_data.pm1_0_cf1  = pms_make_word(rx_frame[4],  rx_frame[5]);
                current_data.pm2_5_cf1  = pms_make_word(rx_frame[6],  rx_frame[7]);
                current_data.pm10_cf1   = pms_make_word(rx_frame[8],  rx_frame[9]);
                current_data.pm1_0_atm  = pms_make_word(rx_frame[10], rx_frame[11]);
                current_data.pm2_5_atm  = pms_make_word(rx_frame[12], rx_frame[13]);
                current_data.pm10_atm   = pms_make_word(rx_frame[14], rx_frame[15]);
                current_data.cnt_0_3um  = pms_make_word(rx_frame[16], rx_frame[17]);
                current_data.cnt_0_5um  = pms_make_word(rx_frame[18], rx_frame[19]);
                current_data.cnt_1_0um  = pms_make_word(rx_frame[20], rx_frame[21]);
                current_data.cnt_2_5um  = pms_make_word(rx_frame[22], rx_frame[23]);
                current_data.cnt_5_0um  = pms_make_word(rx_frame[24], rx_frame[25]);
                current_data.cnt_10_0um = pms_make_word(rx_frame[26], rx_frame[27]);
                current_data.version     = rx_frame[28];
                current_data.error_code  = rx_frame[29];
                current_data.checksum_ok = 1;
                data_ready = 1;
            }
            state = PMS_STATE_IDLE;
        }
        break;
    }
}

/* HAL UART Rx Event Callback — IDLE中断/半满/全满时触发 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart != &huart3) return;
    /* 逐个字节喂给帧解析状态机 */
    for (uint16_t i = 0; i < Size; i++) {
        pms_parse_byte(rx_dma_buf[i]);
    }
    /* 重新启动 DMA 接收 */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_dma_buf, PMS_DMA_BUF_SIZE);
}

int8_t PMS7003_Init(void) {
    state = PMS_STATE_IDLE;
    rx_pos = 0;
    data_ready = 0;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_dma_buf, PMS_DMA_BUF_SIZE);
    return 0;
}

int8_t PMS7003_GetData(pms7003_data_t *data) {
    if (!data_ready) return -1;
    __disable_irq();
    *data = current_data;
    data_ready = 0;
    __enable_irq();
    return 0;
}

uint8_t PMS7003_IsDataReady(void) {
    return data_ready;
}
```

- [ ] **Step 3: 添加 USART3 RxEvent 回调到 stm32h5xx_it.c 或 HAL MSP 回调**

在 USART3 中断中逐字节解析。由于使用了 `HAL_UARTEx_ReceiveToIdle_DMA`，HAL 库会在 DMA 完成或 IDLE 时调用 `HAL_UARTEx_RxEventCallback`。需要在回调中逐字节喂给状态机。

修改 `pms7003.c` 增加 RxEventCallback 实现，确保在 main.c 中包含 pms7003.h。

- [ ] **Step 4: 验证编译**

在 Keil MDK 中添加 `Core/Src/drivers/pms7003.c` 到工程，确认编译无错误。

---

### Task 5: ACS712 电流检测驱动 (hall.c)

**Files:**
- Create: `project/Core/Inc/drivers/hall.h`
- Create: `project/Core/Src/drivers/hall.c`

- [ ] **Step 1: 编写 hall.h**

> 写到 `project/Core/Inc/drivers/hall.h`

```c
#ifndef __HALL_H
#define __HALL_H

#include <stdint.h>

int8_t  Hall_Init(void);
float   Hall_GetCurrent(void);
float   Hall_GetInstantCurrent(void);
void    Hall_SetWindowSize(uint16_t samples);
uint8_t Hall_IsOvercurrent(float threshold);

#endif /* __HALL_H */
```

- [ ] **Step 2: 编写 hall.c**

> 写到 `project/Core/Src/drivers/hall.c`

```c
#include "drivers/hall.h"
#include "config.h"
#include "adc.h"
#include "gpdma.h"

static volatile uint16_t adc_buf[HALL_ADC_BUF_SIZE];
static volatile uint16_t window_size = HALL_WINDOW_SIZE;
static volatile uint32_t adc_sum = 0;
static volatile uint16_t sample_count = 0;
static volatile uint16_t buf_index = 0;
static volatile uint8_t  buf_full = 0;

static float adc_to_current(uint16_t adc_val) {
    float v_adc = ((float)adc_val / HALL_ADC_RES) * HALL_ADC_VREF;
    float v_actual = v_adc * HALL_DIVIDER_RATIO;
    return (v_actual - HALL_OFFSET_V_5V) / HALL_SENSITIVITY;
}

int8_t Hall_Init(void) {
    for (uint16_t i = 0; i < HALL_ADC_BUF_SIZE; i++) {
        adc_buf[i] = 0;
    }
    adc_sum = 0;
    sample_count = 0;
    buf_index = 0;
    buf_full = 0;
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, HALL_ADC_BUF_SIZE);
    return 0;
}

float Hall_GetCurrent(void) {
    if (sample_count == 0) return 0.0f;

    uint32_t sum;
    uint16_t count;
    __disable_irq();
    sum = adc_sum;
    count = sample_count;
    __enable_irq();

    return adc_to_current((uint16_t)(sum / count));
}

float Hall_GetInstantCurrent(void) {
    uint16_t idx;
    __disable_irq();
    idx = (buf_index == 0) ? HALL_ADC_BUF_SIZE - 1 : buf_index - 1;
    uint16_t val = adc_buf[idx];
    __enable_irq();
    return adc_to_current(val);
}

void Hall_SetWindowSize(uint16_t samples) {
    if (samples > HALL_ADC_BUF_SIZE) samples = HALL_ADC_BUF_SIZE;
    if (samples < 1) samples = 1;
    __disable_irq();
    window_size = samples;
    __enable_irq();
}

uint8_t Hall_IsOvercurrent(float threshold) {
    return (Hall_GetInstantCurrent() > threshold) ? 1 : 0;
}

/* DMA 半传输完成回调 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc != &hadc1) return;
    /* 上半缓冲区可用，更新滑动窗口 */
    for (uint16_t i = 0; i < HALL_ADC_BUF_SIZE / 2; i++) {
        uint16_t old_val = adc_buf[buf_index];
        uint16_t new_val = adc_buf[i];

        if (buf_full) {
            adc_sum -= old_val;
        }
        adc_sum += new_val;
        adc_buf[buf_index] = new_val;

        buf_index++;
        if (buf_index >= window_size) {
            buf_index = 0;
            buf_full = 1;
        }
        if (sample_count < window_size) {
            sample_count++;
        }
    }
}

/* DMA 传输完成回调 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc != &hadc1) return;
    /* 下半缓冲区可用 */
    for (uint16_t i = HALL_ADC_BUF_SIZE / 2; i < HALL_ADC_BUF_SIZE; i++) {
        uint16_t old_val = adc_buf[buf_index];
        uint16_t new_val = adc_buf[i];

        if (buf_full) {
            adc_sum -= old_val;
        }
        adc_sum += new_val;
        adc_buf[buf_index] = new_val;

        buf_index++;
        if (buf_index >= window_size) {
            buf_index = 0;
            buf_full = 1;
        }
        if (sample_count < window_size) {
            sample_count++;
        }
    }
}
```

- [ ] **Step 3: 验证编译**

在 Keil MDK 中添加 `Core/Src/drivers/hall.c` 到工程，确认编译无错误。

---

### Task 6: 传感器融合层 (sensor_hub)

**Files:**
- Create: `project/Core/Inc/app/sensor_hub.h`
- Create: `project/Core/Src/app/sensor_hub.c`

- [ ] **Step 1: 编写 sensor_hub.h**

> 写到 `project/Core/Inc/app/sensor_hub.h`

```c
#ifndef __SENSOR_HUB_H
#define __SENSOR_HUB_H

#include "config.h"

int8_t SensorHub_Init(void);
void   SensorHub_Update(sensor_data_t *out);

#endif /* __SENSOR_HUB_H */
```

- [ ] **Step 2: 编写 sensor_hub.c**

> 写到 `project/Core/Src/app/sensor_hub.c`

```c
#include "app/sensor_hub.h"
#include "drivers/sht40.h"
#include "drivers/sgp40.h"
#include "drivers/pms7003.h"
#include "drivers/hall.h"
#include "cmsis_os2.h"

int8_t SensorHub_Init(void) {
    int8_t ret;

    ret = SHT40_Init();
    if (ret != 0) return ret;

    ret = SGP40_Init();
    if (ret != 0) return ret;

    ret = PMS7003_Init();
    if (ret != 0) return ret;

    ret = Hall_Init();
    if (ret != 0) return ret;

    return 0;
}

void SensorHub_Update(sensor_data_t *out) {
    float temp_c   = 25.0f;
    float hum_pct  = 50.0f;
    float current  = 0.0f;
    uint16_t voc_raw = 0;
    uint16_t pm2_5 = 0;
    uint16_t pm10  = 0;

    /* 1. SHT40 温湿度 — SGP40 补偿前置数据 */
    if (SHT40_Read(&temp_c, &hum_pct) != 0) {
        temp_c  = 25.0f;
        hum_pct = 50.0f;
    }

    /* 2. SGP40 VOC — 依赖温湿度 */
    if (SGP40_MeasureRaw(temp_c, hum_pct, &voc_raw) != 0) {
        voc_raw = 0;
    }

    /* 3. PMS7003 — 独立 */
    pms7003_data_t pms;
    if (PMS7003_GetData(&pms) == 0) {
        pm2_5 = pms.pm2_5_atm;
        pm10  = pms.pm10_atm;
    }

    /* 4. ACS712 — 独立 */
    current = Hall_GetCurrent();

    /* 汇总 */
    out->temperature = temp_c;
    out->humidity    = hum_pct;
    out->voc_raw     = voc_raw;
    out->voc_index   = SGP40_RawToVOCIndex(voc_raw);
    out->pm2_5       = pm2_5;
    out->pm10        = pm10;
    out->current     = current;
    out->power       = current * 12.0f; /* 假设 12V 供电 */
    out->last_update = osKernelGetTickCount();
}
```

- [ ] **Step 3: 验证编译**

在 Keil MDK 中添加 `Core/Src/app/sensor_hub.c` 到工程，确认编译无错误。

---

### Task 7: FreeRTOS 任务集成

**Files:**
- Modify: `project/Core/Src/app_freertos.c` — 创建 SensorTask
- Modify: `project/Core/Src/stm32h5xx_it.c` — 注册 USART3 IDLE 回调给 PMS7003

- [ ] **Step 1: 修改 app_freertos.c — 创建任务**

在 `project/Core/Src/app_freertos.c` 中修改：

① 顶部 Includes 区域（`/* USER CODE BEGIN Includes */` 后）添加：

```c
#include "app/sensor_hub.h"
#include "drivers/pms7003.h"
```

② 在 `MX_FREERTOS_Init()` 中创建 SensorTask（`/* USER CODE BEGIN RTOS_THREADS */` 后）：

```c
  osThreadId_t sensorTaskHandle;
  const osThreadAttr_t sensorTask_attr = {
    .name = "SensorTask",
    .priority = osPriorityHigh,
    .stack_size = 512 * 4
  };
  sensorTaskHandle = osThreadNew(SensorTask, NULL, &sensorTask_attr);
```

③ 添加 `StartDefaultTask` 之前定义全局数据和 SensorTask 函数（`/* USER CODE BEGIN Application */` 后）：

```c
static sensor_data_t g_sensor_data;

void SensorTask(void *argument) {
  SensorHub_Init();

  for (;;) {
    SensorHub_Update(&g_sensor_data);
    osDelay(1000);
  }
}
```

④ `StartDefaultTask` 函数体改为空（保留循环）：

```c
void StartDefaultTask(void *argument) {
  for (;;) {
    osDelay(1000);
  }
}
```

- [ ] **Step 2: 修改 stm32h5xx_it.c — 添加 USART3 中断处理**

在 `project/Core/Src/stm32h5xx_it.c` 的 `USART3_IRQHandler` 中，确认 HAL 默认处理即可——`HAL_UARTEx_ReceiveToIdle_DMA` 由 HAL 内部处理 IDLE 中断，会在 RxEventCallback 中回调。

但如果需要逐字节解析（DMA 接收 1 字节 + IDLE），则修改 `HAL_UARTEx_RxEventCallback` 在 pms7003.c 中已经实现，HAL 会自动调用。

- [ ] **Step 3: 验证编译**

完整编译工程，确认所有源文件编译通过，无错误无警告。

- [ ] **Step 4: 提交**

```bash
git add -A
git commit -m "feat: add SHT40, SGP40, PMS7003, ACS712 drivers + sensor hub fusion layer"
```
