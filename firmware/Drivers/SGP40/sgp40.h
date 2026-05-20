#ifndef SGP40_H
#define SGP40_H

#include <stdint.h>

#define SGP40_I2C_ADDR          0x59
#define SGP40_CMD_SELF_TEST     0x280E
#define SGP40_CMD_MEASURE_RAW   0x260F
#define SGP40_CMD_HEATER_OFF    0x3615
#define SGP40_CMD_GET_SERIAL    0x3682

typedef struct {
    uint16_t raw_resistance;
    float    voc_index;      /* 0-500, 需要转换算法 */
    float    temperature_c;   /* 用于温度补偿 */
    float    humidity_pct;   /* 用于湿度补偿 */
    uint8_t  valid;
} sgp40_reading_t;

uint8_t SGP40_Init(void);
uint8_t SGP40_SelfTest(void);
uint8_t SGP40_GetSerial(uint64_t* serial);
uint8_t SGP40_MeasureRaw(sgp40_reading_t* reading);
uint8_t SGP40_MeasureWithCompensation(sgp40_reading_t* reading,
                                       float temperature_c,
                                       float humidity_pct);
uint8_t SGP40_HeaterOff(void);

/* VOC Index 估算 (基于原始值和温湿度补偿) */
float SGP40_ComputeVOCIndex(uint16_t raw, float temp_c, float rh_pct);

/* CRC-8 for SGP40 */
uint8_t SGP40_CRC8(const uint8_t* data, uint8_t len);

#endif /* SGP40_H */
