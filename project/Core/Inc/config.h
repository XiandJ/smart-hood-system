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
#define HALL_SENSITIVITY   0.0925f   /* 分压后等效灵敏度 = 0.185/2 */
#define HALL_OFFSET_V      1.25f     /* 分压后零电流电压 = 2.5V/2 */
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
