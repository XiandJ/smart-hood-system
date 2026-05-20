#include "openmv_bridge.h"
#include "stm32h5xx_hal.h"
#include <string.h>

extern UART_HandleTypeDef huart1;

OpenMV_Bridge_t g_openmv;

/* CRC16-IBM (Modbus) 查找表 */
static const uint16_t crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

uint16_t OpenMV_CRC16(const uint8_t* data, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc16_table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}

void OpenMV_Init(void) {
    memset(&g_openmv, 0, sizeof(g_openmv));
    g_openmv.connected = 0;

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
}

/* 逐字节解析 (在UART RX中断中调用) */
void OpenMV_ProcessByte(uint8_t byte) {
    if (!g_openmv.rx_header_found) {
        /* 搜索帧头 0xAA 0x55 */
        if (g_openmv.rx_idx == 0 && byte == 0xAA) {
            g_openmv.rx_buffer[0] = byte;
            g_openmv.rx_idx = 1;
        } else if (g_openmv.rx_idx == 1 && byte == 0x55) {
            g_openmv.rx_buffer[1] = byte;
            g_openmv.rx_idx = 2;
            g_openmv.rx_header_found = 1;
        } else {
            g_openmv.rx_idx = 0;
        }
    } else {
        g_openmv.rx_buffer[g_openmv.rx_idx++] = byte;

        /* 帧头(2) + CMD(1) + LEN(1) + CRC16(2) = 最小6字节帧头部分 */
        if (g_openmv.rx_idx >= 6) {
            uint8_t payload_len = g_openmv.rx_buffer[3];
            uint8_t total_len = 6 + payload_len; /* HEADER(2)+CMD(1)+LEN(1)+CRC(2)+payload */

            if (g_openmv.rx_idx >= total_len) {
                /* 校验CRC */
                uint16_t crc_rx = (g_openmv.rx_buffer[total_len - 2])
                                | (g_openmv.rx_buffer[total_len - 1] << 8);
                uint16_t crc_calc = OpenMV_CRC16(&g_openmv.rx_buffer[2], total_len - 4);

                if (crc_rx == crc_calc) {
                    uint8_t cmd = g_openmv.rx_buffer[2];
                    g_openmv.rx_frame.crc_ok = 1;

                    switch (cmd) {
                    case OMV_CMD_SMOKE_DATA:
                        g_openmv.rx_frame.smoke.smoke_detected  = g_openmv.rx_buffer[4];
                        g_openmv.rx_frame.smoke.smoke_level     = g_openmv.rx_buffer[5];
                        g_openmv.rx_frame.smoke.smoke_area_px   = (g_openmv.rx_buffer[6] << 8) | g_openmv.rx_buffer[7];
                        break;

                    case OMV_CMD_PERSON_DATA:
                        g_openmv.rx_frame.person.person_detected = g_openmv.rx_buffer[4];
                        g_openmv.rx_frame.person.person_count   = g_openmv.rx_buffer[5];
                        g_openmv.rx_frame.person.bbox_x         = (g_openmv.rx_buffer[6] << 8)  | g_openmv.rx_buffer[7];
                        g_openmv.rx_frame.person.bbox_y         = (g_openmv.rx_buffer[8] << 8)  | g_openmv.rx_buffer[9];
                        g_openmv.rx_frame.person.bbox_w         = (g_openmv.rx_buffer[10] << 8) | g_openmv.rx_buffer[11];
                        g_openmv.rx_frame.person.bbox_h         = (g_openmv.rx_buffer[12] << 8) | g_openmv.rx_buffer[13];
                        g_openmv.rx_frame.person.confidence     = g_openmv.rx_buffer[14];
                        break;

                    case OMV_CMD_GESTURE_DATA:
                        g_openmv.rx_frame.gesture.gesture_id    = g_openmv.rx_buffer[4];
                        g_openmv.rx_frame.gesture.confidence    = g_openmv.rx_buffer[5];
                        break;

                    case OMV_CMD_HEARTBEAT:
                        g_openmv.connected = 1;
                        g_openmv.heartbeat_cnt++;
                        break;

                    default:
                        break;
                    }

                    g_openmv.rx_frame.timestamp_ms = HAL_GetTick();
                    g_openmv.last_rx_tick = g_openmv.rx_frame.timestamp_ms;

                } else {
                    g_openmv.frame_error_cnt++;
                }

                /* 重置接收状态 */
                g_openmv.rx_idx = 0;
                g_openmv.rx_header_found = 0;
            }
        }
    }
}

/* UART接收中断回调 */
void OpenMV_UART_RxCallback(uint8_t byte) {
    OpenMV_ProcessByte(byte);
}

/* 发送命令到OpenMV */
void OpenMV_SendCommand(uint8_t cmd, uint8_t* payload, uint8_t len) {
    uint8_t frame[OPENMV_FRAME_MAX_LEN];
    uint8_t total = 6 + len; /* HEADER(2)+CMD(1)+LEN(1)+CRC(2)+payload */

    if (total > OPENMV_FRAME_MAX_LEN) return;

    frame[0] = 0xAA;
    frame[1] = 0x55;
    frame[2] = cmd;
    frame[3] = len;

    if (payload && len > 0) {
        memcpy(&frame[4], payload, len);
    }

    uint16_t crc = OpenMV_CRC16(&frame[2], 2 + len);
    frame[4 + len]     = crc & 0xFF;
    frame[5 + len]     = (crc >> 8) & 0xFF;

    HAL_UART_Transmit(&huart1, frame, total, 100);
}

void OpenMV_SetMode(OpenMV_Mode_t mode) {
    uint8_t payload = (uint8_t)mode;
    OpenMV_SendCommand(OMV_CMD_SET_MODE, &payload, 1);
}

/* UART IRQ Handler - 在stm32h5xx_it.c中调用 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    if (huart->Instance == USART3) {
        /* PMS7003的数据由独立ISR处理 */
    }
}
