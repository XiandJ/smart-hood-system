#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// ==================== 帧类型定义 ====================
// 与 STM32H5 端协议保持一致
enum FrameType : uint8_t {
    FRAME_TYPE_SENSOR   = 0x01,  // 传感器数据
    FRAME_TYPE_CONTROL  = 0x02,  // 控制命令
    FRAME_TYPE_STATUS   = 0x03,  // 系统状态
    FRAME_TYPE_CAMERA   = 0x10,  // OpenMV 图像帧
    FRAME_TYPE_HEARTBEAT = 0xFF  // 心跳
};

// ==================== 帧头结构 ====================
// [AA 55] [type] [len_hi] [len_lo] [payload...] [crc_hi] [crc_lo]
#pragma pack(push, 1)
struct FrameHeader {
    uint8_t  sync1;      // 0xAA
    uint8_t  sync2;      // 0x55
    uint8_t  type;       // FrameType
    uint16_t length;     // payload 长度 (不含帧头和CRC)
};

struct SensorPayload {
    float    temperature;   // 温度 (°C)
    float    humidity;      // 湿度 (%)
    uint16_t voc;           // SGP40 VOC 指数
    uint16_t pm25;          // PMS7003 PM2.5 (μg/m³)
    float    current;       // 霍尔电流 (A)
    uint8_t  fan_level;     // 风机档位 0-5
    uint8_t  state;         // 系统状态枚举
    uint8_t  ai_result;     // AI 推理结果
};
#pragma pack(pop)

// ==================== CRC16-CCITT ====================
static inline uint16_t crc16_ccitt(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

#endif
