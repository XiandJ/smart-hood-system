#include "app/sensor_hub.h"
#include "drivers/sht40.h"
#include "drivers/sgp40.h"
#include "drivers/pms7003.h"
#include "drivers/hall.h"
#include "cmsis_os2.h"

int8_t SensorHub_Init(void) {
    int8_t ret;

    ret = SHT40_Init();
    if (ret != 0) return ret;

    ret = SGP40_Init();
    if (ret != 0) return ret;

    ret = PMS7003_Init();
    if (ret != 0) return ret;

    ret = Hall_Init();
    if (ret != 0) return ret;

    return 0;
}

void SensorHub_Update(sensor_data_t *out) {
    float temp_c   = 25.0f;
    float hum_pct  = 50.0f;
    float current  = 0.0f;
    uint16_t voc_raw = 0;
    uint16_t pm2_5 = 0;
    uint16_t pm10  = 0;

    /* 1. SHT40 temperature/humidity — prerequisite for SGP40 compensation */
    if (SHT40_Read(&temp_c, &hum_pct) != 0) {
        temp_c  = 25.0f;
        hum_pct = 50.0f;
    }

    /* 2. SGP40 VOC — requires temperature/humidity compensation */
    if (SGP40_MeasureRaw(temp_c, hum_pct, &voc_raw) != 0) {
        voc_raw = 0;
    }

    /* 3. PMS7003 — independent */
    pms7003_data_t pms;
    if (PMS7003_GetData(&pms) == 0) {
        pm2_5 = pms.pm2_5_atm;
        pm10  = pms.pm10_atm;
    }

    /* 4. ACS712 — independent */
    current = Hall_GetCurrent();

    /* fill output struct */
    out->temperature = temp_c;
    out->humidity    = hum_pct;
    out->voc_raw     = voc_raw;
    out->voc_index   = SGP40_RawToVOCIndex(voc_raw);
    out->pm2_5       = pm2_5;
    out->pm10        = pm10;
    out->current     = current;
    out->power       = current * 12.0f;
    out->last_update = osKernelGetTickCount();
}
