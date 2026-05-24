#include "sensirion_i2c_hal.h"
#include "sensirion_common.h"
#include "sensirion_config.h"
#include "i2c.h"

int16_t sensirion_i2c_hal_select_bus(uint8_t bus_idx) {
    (void)bus_idx;
    return 0; /* single bus, no switch needed */
}

void sensirion_i2c_hal_init(void) {
    /* I2C1 already initialized in MX_I2C1_Init() */
}

void sensirion_i2c_hal_free(void) {
    /* no resources to free */
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
