#include "pms7003.h"
#include "stm32h5xx_hal.h"
#include <string.h>

extern UART_HandleTypeDef huart3;

pms7003_ctx_t g_pms7003;

/* PMS7003使用Big-Endian字节序 */
static inline uint16_t be16_to_host(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

uint8_t PMS7003_Init(void) {
    memset(&g_pms7003, 0, sizeof(g_pms7003));
    g_pms7003.state = PMS_PARSE_IDLE;
    g_pms7003.last_rx_tick = HAL_GetTick();

    /* 启动UART接收中断 */
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
    return 1;
}

void PMS7003_FeedByte(uint8_t byte) {
    g_pms7003.last_rx_tick = HAL_GetTick();

    switch (g_pms7003.state) {
    case PMS_PARSE_IDLE:
        if (byte == PMS7003_START1) {
            g_pms7003.state = PMS_PARSE_START1;
        }
        break;

    case PMS_PARSE_START1:
        if (byte == PMS7003_START2) {
            g_pms7003.state = PMS_PARSE_START2;
            g_pms7003.idx = 0;
        } else if (byte != PMS7003_START1) {
            g_pms7003.state = PMS_PARSE_IDLE;
        }
        break;

    case PMS_PARSE_START2:
        g_pms7003.buffer[g_pms7003.idx++] = byte;
        if (g_pms7003.idx >= PMS7003_FRAME_LEN) {
            g_pms7003.state = PMS_PARSE_CHECK;
            g_pms7003.idx = 0;
        }
        break;

    case PMS_PARSE_CHECK:
        /* 实际在完整接收后调用PMS7003_ParseFrame进行校验 */
        g_pms7003.state = PMS_PARSE_IDLE;
        break;

    default:
        g_pms7003.state = PMS_PARSE_IDLE;
        break;
    }
}

/* UART接收中断回调 - 放入HAL_UART_RxCpltCallback中调用 */
void PMS7003_UART_IRQHandler(uint8_t byte) {
    PMS7003_FeedByte(byte);
}

uint8_t PMS7003_ParseFrame(const uint8_t* data, pms7003_reading_t* reading) {
    if (!data || !reading) return 0;

    reading->valid = 0;
    reading->checksum_ok = 0;

    /* 校验帧头 */
    if (data[0] != PMS7003_START1 || data[1] != PMS7003_START2) {
        return 0;
    }

    /* 校验和: 前30字节之和取低16位 */
    uint16_t sum = 0;
    for (uint8_t i = 0; i < 30; i++) {
        sum += data[i];
    }
    uint16_t checksum = be16_to_host(&data[30]);
    if (sum != checksum) {
        return 0; /* 校验失败，但返回数据(标记为校验无效) */
    }
    reading->checksum_ok = 1;

    reading->pm1_0_cf1 = be16_to_host(&data[4]);
    reading->pm2_5_cf1 = be16_to_host(&data[6]);
    reading->pm10_cf1  = be16_to_host(&data[8]);
    reading->pm1_0_atm = be16_to_host(&data[10]);
    reading->pm2_5_atm = be16_to_host(&data[12]);
    reading->pm10_atm  = be16_to_host(&data[14]);
    reading->n0_3 = be16_to_host(&data[16]);
    reading->n0_5 = be16_to_host(&data[18]);
    reading->n1_0 = be16_to_host(&data[20]);
    reading->n2_5 = be16_to_host(&data[22]);
    reading->n5_0 = be16_to_host(&data[24]);
    reading->n10  = be16_to_host(&data[26]);

    /* 版本号和错误码 */
    /* uint8_t fw_ver = data[28]; */
    /* uint8_t error_code = data[29]; */

    reading->valid = 1;
    return 1;
}

uint8_t PMS7003_ReadActive(pms7003_reading_t* reading) {
    return PMS7003_ParseFrame(g_pms7003.buffer, reading);
}

void PMS7003_SetMode(uint8_t mode) {
    uint8_t cmd[7];
    cmd[0] = 0x42;
    cmd[1] = 0x4D;
    cmd[2] = 0xE1;
    cmd[3] = mode;
    cmd[4] = 0x00;
    uint16_t sum = 0x42 + 0x4D + 0xE1 + mode;
    cmd[5] = (sum >> 8) & 0xFF;
    cmd[6] = sum & 0xFF;

    HAL_UART_Transmit(&huart3, cmd, 7, 100);
}

uint8_t PMS7003_Sleep(void) {
    uint8_t cmd[7];
    cmd[0] = 0x42; cmd[1] = 0x4D;
    cmd[2] = 0xE4; cmd[3] = 0x00; cmd[4] = 0x00;
    uint16_t sum = 0x42 + 0x4D + 0xE4;
    cmd[5] = (sum >> 8) & 0xFF; cmd[6] = sum & 0xFF;
    return (HAL_UART_Transmit(&huart3, cmd, 7, 100) == HAL_OK) ? 1 : 0;
}

uint8_t PMS7003_Wakeup(void) {
    uint8_t cmd[7];
    cmd[0] = 0x42; cmd[1] = 0x4D;
    cmd[2] = 0xE4; cmd[3] = 0x01; cmd[4] = 0x00;
    uint16_t sum = 0x42 + 0x4D + 0xE4 + 0x01;
    cmd[5] = (sum >> 8) & 0xFF; cmd[6] = sum & 0xFF;
    return (HAL_UART_Transmit(&huart3, cmd, 7, 100) == HAL_OK) ? 1 : 0;
}
