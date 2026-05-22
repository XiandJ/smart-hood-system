#ifndef UART_SENSOR_H
#define UART_SENSOR_H

#include <Arduino.h>
#include "config.h"
#include "protocol.h"

// 传感器数据接收模块
// 从 STM32H5 通过 UART2 接收 CRC16 帧，解析出传感器数据

// 最新传感器数据（全局共享，由 network_task 消费）
struct SensorData {
    float    temperature;
    float    humidity;
    uint16_t voc;
    uint16_t pm25;
    float    current;
    uint8_t  fan_level;
    uint8_t  state;
    uint8_t  ai_result;
    uint32_t last_update;   // 最后更新时间 ms
    bool     valid;         // 数据是否有效
};

extern SensorData g_sensorData;
extern SemaphoreHandle_t g_sensorMutex;

// UART 接收缓冲
static uint8_t  _sensBuf[FRAME_MAX_LEN];
static uint16_t _sensBufIdx = 0;
static HardwareSerial _sensSerial(UART_SENS_NUM);

inline void uart_sensor_begin() {
    _sensSerial.begin(UART_SENS_BAUD, SERIAL_8N1, UART_SENS_RX, UART_SENS_TX);
    Serial.println("[UART-SENS] 传感器串口已初始化");
}

// 在 loop 或 task 中调用，非阻塞解析
inline void uart_sensor_update() {
    while (_sensSerial.available()) {
        uint8_t b = _sensSerial.read();

        // 简单状态机: 寻找帧头 -> 读取帧头 -> 读取payload -> 校验CRC
        _sensBuf[_sensBufIdx++] = b;

        // 至少需要帧头(3字节) + 长度(2字节) = 5字节才能判断
        if (_sensBufIdx < 5) continue;

        // 检查帧头
        if (_sensBuf[0] != FRAME_HEADER_1 || _sensBuf[1] != FRAME_HEADER_2) {
            // 帧头不匹配，左移一个字节
            memmove(_sensBuf, _sensBuf + 1, --_sensBufIdx);
            continue;
        }

        // 读取 payload 长度
        FrameHeader *hdr = (FrameHeader *)_sensBuf;
        uint16_t totalLen = sizeof(FrameHeader) + hdr->length + 2;  // +2 for CRC

        if (totalLen > FRAME_MAX_LEN) {
            _sensBufIdx = 0;
            continue;
        }

        // 数据还没收完
        if (_sensBufIdx < totalLen) continue;

        // CRC 校验
        uint16_t recvCrc = (_sensBuf[totalLen - 2] << 8) | _sensBuf[totalLen - 1];
        uint16_t calcCrc = crc16_ccitt(_sensBuf, totalLen - 2);

        if (recvCrc == calcCrc && hdr->type == FRAME_TYPE_SENSOR) {
            if (hdr->length >= sizeof(SensorPayload)) {
                SensorPayload *payload = (SensorPayload *)(_sensBuf + sizeof(FrameHeader));

                xSemaphoreTake(g_sensorMutex, portMAX_DELAY);
                g_sensorData.temperature = payload->temperature;
                g_sensorData.humidity    = payload->humidity;
                g_sensorData.voc         = payload->voc;
                g_sensorData.pm25        = payload->pm25;
                g_sensorData.current     = payload->current;
                g_sensorData.fan_level   = payload->fan_level;
                g_sensorData.state       = payload->state;
                g_sensorData.ai_result   = payload->ai_result;
                g_sensorData.last_update = millis();
                g_sensorData.valid       = true;
                xSemaphoreGive(g_sensorMutex);
            }
        }

        _sensBufIdx = 0;
    }
}

// 向 STM32 发送控制命令 (风机挡位)
inline void uart_sensor_send_control(uint8_t fan_level) {
    // 帧: [AA 55] [02] [00 01] [fan_level] [crc_hi] [crc_lo]  = 8 bytes
    uint8_t pkt[8];
    pkt[0] = FRAME_HEADER_1;
    pkt[1] = FRAME_HEADER_2;
    pkt[2] = FRAME_TYPE_CONTROL;
    pkt[3] = 0x00;
    pkt[4] = 0x01;
    pkt[5] = fan_level;
    uint16_t ctl_crc = crc16_ccitt(pkt, 6);
    pkt[6] = (uint8_t)(ctl_crc >> 8);
    pkt[7] = (uint8_t)(ctl_crc & 0xFF);

    _sensSerial.write(pkt, 8);
    Serial.printf("[UART-SENS] 发送控制命令: 风机=%d档\n", fan_level);
}

#endif
