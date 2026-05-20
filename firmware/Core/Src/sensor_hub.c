#include "sensor_hub.h"
#include "sht40.h"
#include "sgp40.h"
#include "pms7003.h"
#include "hall.h"
#include "stm32h5xx_hal.h"

SensorHub_t g_sensor_hub;

/* 前向声明 */
static void SensorHub_UpdateDI_DT(void);

void SensorHub_Init(void) {
    memset(&g_sensor_hub, 0, sizeof(g_sensor_hub));
    g_sensor_hub.last_sample_tick = HAL_GetTick();

    /* 初始化各传感器 */
    g_sensor_hub.sht40_status   = SHT40_Init()   ? SENSOR_OK : SENSOR_NOT_FOUND;
    g_sensor_hub.sgp40_status   = SGP40_Init()   ? SENSOR_OK : SENSOR_NOT_FOUND;
    g_sensor_hub.pms7003_status = PMS7003_Init() ? SENSOR_OK : SENSOR_NOT_FOUND;
    g_sensor_hub.hall_status    = Hall_Init()    ? SENSOR_OK : SENSOR_NOT_FOUND;

    g_sensor_hub.all_ok = SensorHub_CheckAllOK();
}

void SensorHub_SampleAll(void) {
    sht40_reading_t sht40_rd;
    sgp40_reading_t sgp40_rd;
    pms7003_reading_t pms7003_rd;
    hall_reading_t hall_rd;
    uint8_t retry;

    /* --- SHT40 --- */
    for (retry = 0; retry < SENSOR_RETRY_MAX; retry++) {
        if (SHT40_Measure(&sht40_rd)) {
            g_sensor_hub.sht40.temperature = sht40_rd.temperature;
            g_sensor_hub.sht40.humidity    = sht40_rd.humidity;
            g_sensor_hub.sht40_status = SENSOR_OK;
            break;
        }
    }
    if (retry >= SENSOR_RETRY_MAX) {
        g_sensor_hub.sht40_status = SENSOR_TIMEOUT;
    }

    /* --- SGP40 (需要温湿度补偿) --- */
    float temp_comp = g_sensor_hub.sht40_status == SENSOR_OK
                      ? g_sensor_hub.sht40.temperature : 25.0f;
    float rh_comp   = g_sensor_hub.sht40_status == SENSOR_OK
                      ? g_sensor_hub.sht40.humidity : 50.0f;

    for (retry = 0; retry < SENSOR_RETRY_MAX; retry++) {
        if (SGP40_MeasureWithCompensation(&sgp40_rd, temp_comp, rh_comp)) {
            g_sensor_hub.sgp40.voc_index = sgp40_rd.voc_index;
            g_sensor_hub.sgp40.voc_raw   = (float)sgp40_rd.raw_resistance;
            g_sensor_hub.sgp40_status = SENSOR_OK;
            break;
        }
    }
    if (retry >= SENSOR_RETRY_MAX) {
        g_sensor_hub.sgp40_status = SENSOR_TIMEOUT;
    }

    /* --- PMS7003 --- */
    if (PMS7003_ReadActive(&pms7003_rd)) {
        g_sensor_hub.pms7003.pm1_0 = pms7003_rd.pm1_0_atm;
        g_sensor_hub.pms7003.pm2_5 = pms7003_rd.pm2_5_atm;
        g_sensor_hub.pms7003.pm10  = pms7003_rd.pm10_atm;
        g_sensor_hub.pms7003.particles_0_3 = pms7003_rd.n0_3;
        g_sensor_hub.pms7003.particles_0_5 = pms7003_rd.n0_5;
        g_sensor_hub.pms7003.particles_1_0 = pms7003_rd.n1_0;
        g_sensor_hub.pms7003.particles_2_5 = pms7003_rd.n2_5;
        g_sensor_hub.pms7003.particles_5_0 = pms7003_rd.n5_0;
        g_sensor_hub.pms7003.particles_10  = pms7003_rd.n10;
        g_sensor_hub.pms7003_status = SENSOR_OK;
    } else {
        g_sensor_hub.pms7003_status = SENSOR_TIMEOUT;
    }

    /* --- 霍尔电流 --- */
    if (Hall_Read(&hall_rd)) {
        g_sensor_hub.hall.current_a = hall_rd.filtered_current_a;
        g_sensor_hub.hall.power_w   = hall_rd.power_w;
        g_sensor_hub.hall_status    = SENSOR_OK;
        SensorHub_UpdateDI_DT();
    } else {
        g_sensor_hub.hall_status = SENSOR_TIMEOUT;
    }

    /* 更新时间戳 */
    uint32_t now = HAL_GetTick();
    g_sensor_hub.last_sample_tick = now;

    /* 生成传感器融合特征 */
    SensorHub_UpdateFusion();

    /* 更新全局状态 */
    g_sensor_hub.all_ok = SensorHub_CheckAllOK();
}

void SensorHub_UpdateFusion(void) {
    sensor_fusion_t* f = &g_sensor_hub.fusion;
    f->features[0] = g_sensor_hub.sht40.temperature;
    f->features[1] = g_sensor_hub.sht40.humidity;
    f->features[2] = g_sensor_hub.sgp40.voc_index;
    f->features[3] = (float)g_sensor_hub.pms7003.pm1_0;
    f->features[4] = (float)g_sensor_hub.pms7003.pm2_5;
    f->features[5] = (float)g_sensor_hub.pms7003.pm10;
    f->features[6] = g_sensor_hub.hall.current_a;
    f->features[7] = g_sensor_hub.hall.di_dt;
    f->timestamp_ms = HAL_GetTick();
}

static void SensorHub_UpdateDI_DT(void) {
    static float prev_current = 0.0f;
    static uint32_t prev_tick = 0;
    static uint8_t first_run = 1;

    uint32_t now = HAL_GetTick();
    float curr = g_sensor_hub.hall.current_a;

    if (first_run) {
        first_run = 0;
        prev_current = curr;
        prev_tick = now;
        g_sensor_hub.hall.di_dt = 0.0f;
        return;
    }

    float dt_s = (float)(now - prev_tick) / 1000.0f;
    if (dt_s > 0.01f) {
        g_sensor_hub.hall.di_dt = (curr - prev_current) / dt_s;
    }
    prev_current = curr;
    prev_tick = now;
}

uint8_t SensorHub_CheckAllOK(void) {
    return (g_sensor_hub.sht40_status   == SENSOR_OK &&
            g_sensor_hub.sgp40_status   == SENSOR_OK &&
            g_sensor_hub.pms7003_status == SENSOR_OK &&
            g_sensor_hub.hall_status    == SENSOR_OK) ? 1 : 0;
}

void SensorHub_PrintDebug(void) {
#if DEBUG_ENABLE
    /* 调试输出通过UART发送传感器数据 */
    extern UART_HandleTypeDef huart2;
    char buf[128];
    int len = snprintf(buf, sizeof(buf),
        "S: T=%.1f RH=%.1f VOC=%.1f PM1=%.1f PM2.5=%.1f PM10=%.1f I=%.2f\r\n",
        g_sensor_hub.sht40.temperature,
        g_sensor_hub.sht40.humidity,
        g_sensor_hub.sgp40.voc_index,
        (float)g_sensor_hub.pms7003.pm1_0,
        (float)g_sensor_hub.pms7003.pm2_5,
        (float)g_sensor_hub.pms7003.pm10,
        g_sensor_hub.hall.current_a);
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, len, 50);
#endif
}
