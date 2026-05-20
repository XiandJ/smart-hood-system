#include "hall.h"
#include "stm32h5xx_hal.h"
#include <math.h>

extern ADC_HandleTypeDef hadc1;

static float ema_current = 0.0f;
static uint8_t ema_initialized = 0;

uint8_t Hall_Init(void) {
    ema_current = 0.0f;
    ema_initialized = 0;
    return 1;
}

float Hall_ADCToCurrent(uint16_t adc_value) {
    float voltage = ((float)adc_value / (float)HALL_ADC_RESOLUTION) * HALL_VREF;
    float current = (voltage - HALL_ZERO_CURRENT_V) / HALL_SENSITIVITY;
    return current;
}

uint8_t Hall_Read(hall_reading_t* reading) {
    if (!reading) return 0;

    uint32_t adc_sum = 0;
    uint16_t adc_max = 0;

    /* 过采样32次求平均 */
    for (uint8_t i = 0; i < HALL_SAMPLE_COUNT; i++) {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK) {
            uint16_t val = HAL_ADC_GetValue(&hadc1);
            adc_sum += val;
            if (val > adc_max) adc_max = val;
        }
        HAL_Delay_us(HALL_SAMPLE_INTERVAL_US);
    }

    reading->raw_current_a = Hall_ADCToCurrent((uint16_t)(adc_sum / HALL_SAMPLE_COUNT));
    reading->saturation = (adc_max >= (HALL_ADC_RESOLUTION - 10)) ? 1 : 0;

    /* EMA滤波 */
    if (!ema_initialized) {
        ema_current = reading->raw_current_a;
        ema_initialized = 1;
    } else {
        ema_current = HALL_EMA_ALPHA * reading->raw_current_a
                    + (1.0f - HALL_EMA_ALPHA) * ema_current;
    }

    /* 计算di/dt */
    reading->previous_current_a = ema_current;
    reading->filtered_current_a = ema_current;
    reading->di_dt = 0.0f; /* 由上层根据时间间隔计算 */

    /* RMS电流 (对于DC电机近似等于滤波后电流) */
    reading->rms_current_a = fabsf(ema_current);

    /* 估算功率 (假设24V供电) */
    reading->power_w = reading->rms_current_a * 24.0f;

    reading->valid = 1;
    return 1;
}

void Hall_ResetFilter(void) {
    ema_current = 0.0f;
    ema_initialized = 0;
}
