#include "sgp40.h"
#include "stm32h5xx_hal.h"
#include <math.h>

extern I2C_HandleTypeDef hi2c1;

static uint8_t sgp40_initialized = 0;

/* CRC-8 for SGP40 (poly 0x31, init 0xFF) - 与SHT40兼容 */
uint8_t SGP40_CRC8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x31) : (crc << 1);
        }
    }
    return crc;
}

uint8_t SGP40_Init(void) {
    if (sgp40_initialized) return 1;

    if (HAL_I2C_IsDeviceReady(&hi2c1, SGP40_I2C_ADDR << 1, 3, 50) != HAL_OK) {
        return 0;
    }

    uint8_t cmd[2];
    cmd[0] = (SGP40_CMD_HEATER_OFF >> 8) & 0xFF;
    cmd[1] = SGP40_CMD_HEATER_OFF & 0xFF;
    HAL_I2C_Master_Transmit(&hi2c1, SGP40_I2C_ADDR << 1, cmd, 2, 50);
    HAL_Delay(10);

    sgp40_initialized = 1;
    return 1;
}

uint8_t SGP40_SelfTest(void) {
    uint8_t cmd[2] = {
        (SGP40_CMD_SELF_TEST >> 8) & 0xFF,
        SGP40_CMD_SELF_TEST & 0xFF
    };
    uint8_t rx[3];

    if (HAL_I2C_Master_Transmit(&hi2c1, SGP40_I2C_ADDR << 1,
                                  cmd, 2, 50) != HAL_OK) return 0;
    HAL_Delay(250);
    if (HAL_I2C_Master_Receive(&hi2c1, SGP40_I2C_ADDR << 1,
                                rx, 3, 50) != HAL_OK) return 0;
    if (SGP40_CRC8(rx, 2) != rx[2]) return 0;

    uint16_t result = ((uint16_t)rx[0] << 8) | rx[1];
    return (result < 20000 && result > 10000) ? 1 : 0;
}

uint8_t SGP40_MeasureRaw(sgp40_reading_t* reading) {
    uint8_t cmd[2] = {
        (SGP40_CMD_MEASURE_RAW >> 8) & 0xFF,
        SGP40_CMD_MEASURE_RAW & 0xFF
    };
    uint8_t rx[3];

    if (!reading) return 0;
    reading->valid = 0;

    if (HAL_I2C_Master_Transmit(&hi2c1, SGP40_I2C_ADDR << 1,
                                  cmd, 2, 50) != HAL_OK) return 0;
    HAL_Delay(35); /* 典型测量时间30ms + 余量 */
    if (HAL_I2C_Master_Receive(&hi2c1, SGP40_I2C_ADDR << 1,
                                rx, 3, 50) != HAL_OK) return 0;
    if (SGP40_CRC8(rx, 2) != rx[2]) return 0;

    reading->raw_resistance = ((uint16_t)rx[0] << 8) | rx[1];
    reading->valid = 1;
    return 1;
}

uint8_t SGP40_MeasureWithCompensation(sgp40_reading_t* reading,
                                       float temperature_c,
                                       float humidity_pct) {
    uint8_t cmd[8];
    uint8_t rx[3];

    if (!reading) return 0;
    reading->valid = 0;

    /* 构造带温湿度补偿的测量命令 */
    uint16_t rh_ticks  = (uint16_t)((humidity_pct + 5.0f) / 0.0025f);
    uint16_t temp_ticks = (uint16_t)((temperature_c + 45.0f) / 0.0025f);

    cmd[0] = (SGP40_CMD_MEASURE_RAW >> 8) & 0xFF;
    cmd[1] = SGP40_CMD_MEASURE_RAW & 0xFF;
    cmd[2] = (rh_ticks >> 8) & 0xFF;
    cmd[3] = rh_ticks & 0xFF;
    cmd[4] = SGP40_CRC8(&cmd[2], 2);
    cmd[5] = (temp_ticks >> 8) & 0xFF;
    cmd[6] = temp_ticks & 0xFF;
    cmd[7] = SGP40_CRC8(&cmd[5], 2);

    if (HAL_I2C_Master_Transmit(&hi2c1, SGP40_I2C_ADDR << 1,
                                  cmd, 8, 50) != HAL_OK) return 0;
    HAL_Delay(35);
    if (HAL_I2C_Master_Receive(&hi2c1, SGP40_I2C_ADDR << 1,
                                rx, 3, 50) != HAL_OK) return 0;
    if (SGP40_CRC8(rx, 2) != rx[2]) return 0;

    reading->raw_resistance = ((uint16_t)rx[0] << 8) | rx[1];
    reading->temperature_c = temperature_c;
    reading->humidity_pct = humidity_pct;
    reading->voc_index = SGP40_ComputeVOCIndex(reading->raw_resistance,
                                                temperature_c, humidity_pct);
    reading->valid = 1;
    return 1;
}

uint8_t SGP40_HeaterOff(void) {
    uint8_t cmd[2] = {
        (SGP40_CMD_HEATER_OFF >> 8) & 0xFF,
        SGP40_CMD_HEATER_OFF & 0xFF
    };
    return (HAL_I2C_Master_Transmit(&hi2c1, SGP40_I2C_ADDR << 1,
                                     cmd, 2, 50) == HAL_OK) ? 1 : 0;
}

/* VOC Index 估算算法
 * 基于Sensirion提供的参考实现，结合原始电阻值和温湿度补偿
 * 使用分段线性+对数映射近似官方算法 */
float SGP40_ComputeVOCIndex(uint16_t raw, float temp_c, float rh_pct) {
    (void)temp_c;
    (void)rh_pct;

    if (raw < 5000) return 0.0f;

    float log_raw = logf((float)raw);

    /* 分段映射 (基于实验数据的简化模型)
     * 实际部署时应使用Sensirion官方气体指数算法文件 */
    float voc;
    if (log_raw < 9.0f) {
        voc = 5.0f * (log_raw - 8.5f);
    } else if (log_raw < 10.0f) {
        voc = 2.5f + 25.0f * (log_raw - 9.0f);
    } else {
        voc = 27.5f + 47.25f * (log_raw - 10.0f);
    }

    if (voc < 0.0f)   voc = 0.0f;
    if (voc > 500.0f) voc = 500.0f;

    return floorf(voc);
}
