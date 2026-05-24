#include "drivers/hall.h"
#include "config.h"
#include "adc.h"

static volatile uint16_t adc_buf[HALL_ADC_BUF_SIZE];
static volatile uint16_t window_size = HALL_WINDOW_SIZE;
static volatile uint32_t adc_sum = 0;
static volatile uint16_t sample_count = 0;
static volatile uint16_t buf_index = 0;
static volatile uint8_t  buf_full = 0;

static float adc_to_current(uint16_t adc_val) {
    float v_adc = ((float)adc_val / HALL_ADC_RES) * HALL_ADC_VREF;
    float v_actual = v_adc * HALL_DIVIDER_RATIO;
    return (v_actual - HALL_OFFSET_V) / HALL_SENSITIVITY;
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

/* DMA half-transfer complete callback */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc != &hadc1) return;
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

/* DMA transfer complete callback */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc != &hadc1) return;
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
