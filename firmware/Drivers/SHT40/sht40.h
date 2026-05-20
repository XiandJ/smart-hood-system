#ifndef SHT40_H
#define SHT40_H

#include <stdint.h>

#define SHT40_I2C_ADDR      0x44
#define SHT40_CMD_MEASURE_H 0xFD    /* 高精度测量 */
#define SHT40_CMD_MEASURE_M 0xF6    /* 中精度 */
#define SHT40_CMD_MEASURE_L 0xE0    /* 低精度 */
#define SHT40_CMD_SERIAL    0x89    /* 读序列号 */
#define SHT40_CMD_RESET     0x94    /* 软复位 */

typedef struct {
    float temperature;
    float humidity;
    uint8_t valid;
} sht40_reading_t;

uint8_t SHT40_Init(void);
uint8_t SHT40_Reset(void);
uint8_t SHT40_ReadSerial(uint32_t* serial);
uint8_t SHT40_Measure(sht40_reading_t* reading);
uint8_t SHT40_MeasureHighPrecision(sht40_reading_t* reading);

/* CRC-8 校验 (多项式 0x31) */
uint8_t SHT40_CRC8(const uint8_t* data, uint8_t len);

#endif /* SHT40_H */
