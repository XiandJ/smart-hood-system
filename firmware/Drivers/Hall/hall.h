#ifndef HALL_H
#define HALL_H

#include <stdint.h>

#define HALL_ADC_RESOLUTION     4095
#define HALL_VREF               3.3f
#define HALL_SENSITIVITY        0.185f   /* ACS712-10A: 185mV/A */
#define HALL_ZERO_CURRENT_V     1.65f    /* Vcc/2 = 1.65V at 0A */

/* 采样配置 */
#define HALL_SAMPLE_COUNT       32       /* 过采样次数 */
#define HALL_SAMPLE_INTERVAL_US 100      /* 采样间隔 */
#define HALL_EMA_ALPHA          0.1f     /* 指数移动平均 */

typedef struct {
    float   raw_current_a;       /* 原始电流值 */
    float   filtered_current_a;  /* EMA滤波后 */
    float   previous_current_a;  /* 上一周期电流 (用于计算di/dt) */
    float   di_dt;               /* 电流变化率 A/s */
    float   power_w;             /* 估算功率 */
    float   rms_current_a;       /* RMS电流 */
    uint8_t valid;
    uint8_t saturation;          /* ADC饱和标志 */
} hall_reading_t;

uint8_t Hall_Init(void);
uint8_t Hall_Read(hall_reading_t* reading);
float Hall_ADCToCurrent(uint16_t adc_value);
void Hall_ResetFilter(void);

#endif /* HALL_H */
