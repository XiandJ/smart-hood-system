#ifndef PMS7003_H
#define PMS7003_H

#include <stdint.h>

/* PMS7003 数据帧长度 */
#define PMS7003_FRAME_LEN   32
#define PMS7003_START1      0x42
#define PMS7003_START2      0x4D

/* 运行模式 */
#define PMS7003_MODE_PASSIVE 0x00
#define PMS7003_MODE_ACTIVE  0x01

/* 控制命令 */
#define PMS7003_CMD_READ         0xE2
#define PMS7003_CMD_CHANGE_MODE  0xE1
#define PMS7003_CMD_SLEEP        0xE4
#define PMS7003_CMD_WAKEUP       0xE4  /* 同命令，不同参数 */

typedef struct {
    /* 标准颗粒物浓度 (μg/m³) - 使用CF=1标准 */
    uint16_t pm1_0_cf1;
    uint16_t pm2_5_cf1;
    uint16_t pm10_cf1;

    /* 大气环境颗粒物浓度 (μg/m³) */
    uint16_t pm1_0_atm;
    uint16_t pm2_5_atm;
    uint16_t pm10_atm;

    /* 颗粒物计数 (个/0.1L) */
    uint16_t n0_3;
    uint16_t n0_5;
    uint16_t n1_0;
    uint16_t n2_5;
    uint16_t n5_0;
    uint16_t n10;

    uint8_t  valid;
    uint8_t  checksum_ok;
} pms7003_reading_t;

typedef enum {
    PMS_PARSE_IDLE,
    PMS_PARSE_START1,
    PMS_PARSE_START2,
    PMS_PARSE_DATA,
    PMS_PARSE_CHECK
} pms7003_parse_state_t;

typedef struct {
    pms7003_parse_state_t state;
    uint8_t  buffer[PMS7003_FRAME_LEN];
    uint8_t  idx;
    pms7003_reading_t reading;
    uint32_t last_rx_tick;
} pms7003_ctx_t;

extern pms7003_ctx_t g_pms7003;

uint8_t PMS7003_Init(void);
void PMS7003_FeedByte(uint8_t byte);
uint8_t PMS7003_ParseFrame(const uint8_t* data, pms7003_reading_t* reading);
void PMS7003_SetMode(uint8_t mode);
uint8_t PMS7003_ReadActive(pms7003_reading_t* reading);
uint8_t PMS7003_Sleep(void);
uint8_t PMS7003_Wakeup(void);

#endif /* PMS7003_H */
