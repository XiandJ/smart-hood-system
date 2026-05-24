#include "drivers/sgp40.h"
#include "config.h"
#include "sensirion/sgp40_i2c.h"

static uint8_t initialized = 0;

int8_t SGP40_Init(void) {
    uint16_t test_result;
    int16_t ret = sgp40_execute_self_test(&test_result);
    if (ret != 0) return -1;
    if (test_result != 0xD400) return -2;
    initialized = 1;
    return 0;
}

int8_t SGP40_MeasureRaw(float temp_c, float hum_pct, uint16_t *sraw_voc) {
    if (!initialized) return -1;

    if (hum_pct > 100.0f) hum_pct = 100.0f;
    if (hum_pct < 0.0f)   hum_pct = 0.0f;
    if (temp_c > 130.0f)  temp_c = 130.0f;
    if (temp_c < -45.0f)  temp_c = -45.0f;

    uint16_t rh_ticks  = (uint16_t)((hum_pct / 100.0f) * 65535.0f);
    uint16_t t_ticks   = (uint16_t)(((temp_c + 45.0f) / 175.0f) * 65535.0f);

    int16_t ret = sgp40_measure_raw_signal(rh_ticks, t_ticks, sraw_voc);
    return (ret == 0) ? 0 : -2;
}

int8_t SGP40_HeaterOff(void) {
    int16_t ret = sgp40_turn_heater_off();
    return (ret == 0) ? 0 : -1;
}

uint16_t SGP40_RawToVOCIndex(uint16_t sraw_voc) {
    if (sraw_voc < 10000) return 0;
    if (sraw_voc > 60000) return 500;
    return (uint16_t)(((float)(sraw_voc - 10000) / 50000.0f) * 500.0f);
}
