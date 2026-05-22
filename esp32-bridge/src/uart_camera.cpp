#include "uart_camera.h"
#include "config.h"
#include "frame_buffer.h"

static HardwareSerial _camSerial(UART_CAM_NUM);
static uint8_t  _camBuf[FRAME_CAM_MAX];
static uint16_t _camBufIdx = 0;
static bool     _inJpegFrame = false;

#define JPEG_SOI_1  0xFF
#define JPEG_SOI_2  0xD8
#define JPEG_EOI_1  0xFF
#define JPEG_EOI_2  0xD9

void uart_camera_begin() {
    _camSerial.begin(UART_CAM_BAUD, SERIAL_8N1, UART_CAM_RX, UART_CAM_TX);
    Serial.println("[UART-CAM] 摄像头串口已初始化");
}

void uart_camera_update() {
    while (_camSerial.available()) {
        uint8_t b = _camSerial.read();

        if (!_inJpegFrame) {
            if (_camBufIdx == 0 && b == JPEG_SOI_1) {
                _camBuf[_camBufIdx++] = b;
            } else if (_camBufIdx == 1 && b == JPEG_SOI_2) {
                _camBuf[_camBufIdx++] = b;
                _inJpegFrame = true;
            } else {
                _camBufIdx = 0;
            }
        } else {
            _camBuf[_camBufIdx++] = b;
            if (_camBufIdx >= 2 &&
                _camBuf[_camBufIdx - 2] == JPEG_EOI_1 &&
                _camBuf[_camBufIdx - 1] == JPEG_EOI_2) {
                g_frameBuffer.push(_camBuf, _camBufIdx);
                _camBufIdx = 0;
                _inJpegFrame = false;
            }
            if (_camBufIdx >= FRAME_CAM_MAX) {
                _camBufIdx = 0;
                _inJpegFrame = false;
            }
        }
    }
}
