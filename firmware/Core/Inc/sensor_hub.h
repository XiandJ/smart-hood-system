#ifndef SENSOR_HUB_H
#define SENSOR_HUB_H

#include "config.h"
#include <stdint.h>

/* 传感器数据结构体 */
typedef struct {
    float temperature;     /* °C */
    float humidity;        /* %RH */
} sht40_data_t;

typedef struct {
    float voc_index;       /* 0-500 */
    float voc_raw;         /* 原始电阻值 */
} sgp40_data_t;

typedef struct {
    uint16_t pm1_0;        /* μg/m³ */
    uint16_t pm2_5;        /* μg/m³ */
    uint16_t pm10;         /* μg/m³ */
    uint16_t particles_0_3;/* 0.3μm以上颗粒数/0.1L */
    uint16_t particles_0_5;
    uint16_t particles_1_0;
    uint16_t particles_2_5;
    uint16_t particles_5_0;
    uint16_t particles_10;
} pms7003_data_t;

typedef struct {
    float current_a;       /* 电流 (A) */
    float power_w;         /* 功率 (W) */
    float di_dt;           /* 电流变化率 (A/s) */
} hall_data_t;

/* 传感器融合数据 (8维特征向量) */
typedef struct {
    float features[8];     /* [T, RH, VOC, PM1.0, PM2.5, PM10, I, dI/dt] */
    uint32_t timestamp_ms;
} sensor_fusion_t;

/* 传感器状态 */
typedef enum {
    SENSOR_OK = 0,
    SENSOR_TIMEOUT,
    SENSOR_CRC_ERROR,
    SENSOR_NOT_FOUND
} sensor_status_t;

/* 传感器管理中心 */
typedef struct {
    sht40_data_t    sht40;
    sgp40_data_t    sgp40;
    pms7003_data_t  pms7003;
    hall_data_t     hall;
    sensor_fusion_t fusion;

    sensor_status_t sht40_status;
    sensor_status_t sgp40_status;
    sensor_status_t pms7003_status;
    sensor_status_t hall_status;

    uint8_t         all_ok;
    uint32_t        last_sample_tick;
} SensorHub_t;

extern SensorHub_t g_sensor_hub;

void SensorHub_Init(void);
void SensorHub_SampleAll(void);
void SensorHub_UpdateFusion(void);
uint8_t SensorHub_CheckAllOK(void);
void SensorHub_PrintDebug(void);

#endif /* SENSOR_HUB_H */
