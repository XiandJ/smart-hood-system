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
