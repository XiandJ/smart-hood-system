#ifndef OPENMV_BRIDGE_H
#define OPENMV_BRIDGE_H

#include "config.h"
#include <stdint.h>

/* ============================================================
 * 通信协议定义
 *
 * 帧格式:
 * [HEADER:2B] [CMD:1B] [LEN:1B] [PAYLOAD:N] [CRC16:2B]
 * HEADER: 0xAA 0x55
 * CRC16: Modbus CRC (payload + CMD + LEN)
 * ============================================================ */

/* 命令码 */
#define OMV_CMD_HEARTBEAT       0x00
#define OMV_CMD_SMOKE_DATA      0x10
#define OMV_CMD_PERSON_DATA     0x11
#define OMV_CMD_GESTURE_DATA    0x12
#define OMV_CMD_FUSION_DATA     0x1F  /* 融合后的综合数据 */

#define OMV_CMD_SET_MODE        0x20
#define OMV_CMD_SET_ROI         0x21
#define OMV_CMD_ENABLE_MODEL    0x22
#define OMV_CMD_DISABLE_MODEL   0x23
#define OMV_CMD_RESET           0xFF

/* 视觉数据类型 */
typedef enum {
    OMV_MODE_SMOKE_ONLY   = 0x01,
    OMV_MODE_PERSON_ONLY  = 0x02,
    OMV_MODE_GESTURE_ONLY = 0x04,
    OMV_MODE_SMOKE_PERSON = 0x03,
    OMV_MODE_ALL          = 0x07
} OpenMV_Mode_t;

/* 烟雾检测结果 */
typedef struct {
    uint8_t smoke_detected;
    uint8_t smoke_level;       /* 0-100 */
    uint16_t smoke_area_px;    /* 烟雾区域像素数 */
} OMV_SmokeData_t;

/* 人员检测结果 */
typedef struct {
    uint8_t person_detected;
    uint8_t person_count;
    uint16_t bbox_x;
    uint16_t bbox_y;
    uint16_t bbox_w;
    uint16_t bbox_h;
    uint8_t confidence;        /* 0-100 */
} OMV_PersonData_t;

/* 手势识别结果 */
typedef struct {
    uint8_t gesture_id;        /* 0=无, 1=上滑, 2=下滑, 3=左滑, 4=右滑, 5=悬停 */
    uint8_t confidence;
} OMV_GestureData_t;

/* OpenMV综合数据帧 */
typedef struct {
    OMV_SmokeData_t   smoke;
    OMV_PersonData_t  person;
    OMV_GestureData_t gesture;
    uint32_t          timestamp_ms;
    uint16_t          crc_ok;
} OpenMV_Frame_t;

/* 通信管理 */
typedef struct {
    OpenMV_Frame_t   rx_frame;
    uint8_t          rx_buffer[OPENMV_FRAME_MAX_LEN];
    uint16_t         rx_idx;
    uint8_t          rx_header_found;
    uint8_t          connected;
    uint32_t         last_rx_tick;
    uint32_t         heartbeat_cnt;
    uint32_t         frame_error_cnt;
} OpenMV_Bridge_t;

extern OpenMV_Bridge_t g_openmv;

void OpenMV_Init(void);
void OpenMV_ProcessByte(uint8_t byte);
void OpenMV_ParseFrame(uint8_t* data, uint8_t len);
void OpenMV_SendCommand(uint8_t cmd, uint8_t* payload, uint8_t len);
void OpenMV_SetMode(OpenMV_Mode_t mode);
uint16_t OpenMV_CRC16(const uint8_t* data, uint8_t len);

void OpenMV_UART_RxCallback(uint8_t byte);

#endif /* OPENMV_BRIDGE_H */
