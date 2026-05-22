#ifndef UART_CAMERA_H
#define UART_CAMERA_H

#include <Arduino.h>
#include "config.h"
#include "frame_buffer.h"

// OpenMV 图像帧接收模块
// 从 OpenMV H7 通过 UART1 接收 JPEG 压缩帧
// 预期帧格式: [AA 55 10 len_hi len_lo] [JPEG数据...] [CRC_hi CRC_lo]
// 简化格式也可: [FF D8 ... FF D9] 直接传输 JPEG SOI~EOI

static HardwareSerial _camSerial(UART_CAM_NUM);
static uint8_t  _camBuf[FRAME_CAM_MAX];
static uint16_t _camBufIdx = 0;
static bool     _inJpegFrame = false;

// JPEG SOI: 0xFF 0xD8, EOI: 0xFF 0xD9
#define JPEG_SOI_1  0xFF
#define JPEG_SOI_2  0xD8
#define JPEG_EOI_1  0xFF
#define JPEG_EOI_2  0xD9

inline void uart_camera_begin() {
    _camSerial.begin(UART_CAM_BAUD, SERIAL_8N1, UART_CAM_RX, UART_CAM_TX);
    Serial.println("[UART-CAM] 摄像头串口已初始化");
}

// 在 task 中调用，非阻塞读取
inline void uart_camera_update() {
    while (_camSerial.available()) {
        uint8_t b = _camSerial.read();

        if (!_inJpegFrame) {
            // 寻找 JPEG SOI
            if (_camBufIdx == 0 && b == JPEG_SOI_1) {
                _camBuf[_camBufIdx++] = b;
            } else if (_camBufIdx == 1 && b == JPEG_SOI_2) {
                _camBuf[_camBufIdx++] = b;
                _inJpegFrame = true;
            } else {
                _camBufIdx = 0;
            }
        } else {
            // 正在接收 JPEG 数据
            _camBuf[_camBufIdx++] = b;

            // 检查 EOI
            if (_camBufIdx >= 2 &&
                _camBuf[_camBufIdx - 2] == JPEG_EOI_1 &&
                _camBuf[_camBufIdx - 1] == JPEG_EOI_2) {
                // 一帧完整的 JPEG
                g_frameBuffer.push(_camBuf, _camBufIdx);
                _camBufIdx = 0;
                _inJpegFrame = false;
            }

            // 溢出保护
            if (_camBufIdx >= FRAME_CAM_MAX) {
                _camBufIdx = 0;
                _inJpegFrame = false;
            }
        }
    }
}

#endif
