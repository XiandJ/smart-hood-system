#include "uart_sensor.h"
#include <string.h>
#include "config.h"
#include "protocol.h"

SensorData g_sensorData = {0};
SemaphoreHandle_t g_sensorMutex = xSemaphoreCreateMutex();

static uint8_t  _sensBuf[FRAME_MAX_LEN];
static uint16_t _sensBufIdx = 0;
static HardwareSerial _sensSerial(UART_SENS_NUM);

void uart_sensor_begin() {
    _sensSerial.begin(UART_SENS_BAUD, SERIAL_8N1, UART_SENS_RX, UART_SENS_TX);
    Serial.println("[UART-SENS] 传感器串口已初始化");
}

void uart_sensor_update() {
    while (_sensSerial.available()) {
        uint8_t b = _sensSerial.read();
        _sensBuf[_sensBufIdx++] = b;

        if (_sensBufIdx < 5) continue;

        if (_sensBuf[0] != FRAME_HEADER_1 || _sensBuf[1] != FRAME_HEADER_2) {
            memmove(_sensBuf, _sensBuf + 1, --_sensBufIdx);
            continue;
        }

        FrameHeader *hdr = (FrameHeader *)_sensBuf;
        uint16_t totalLen = sizeof(FrameHeader) + hdr->length + 2;

        if (totalLen > FRAME_MAX_LEN) {
            _sensBufIdx = 0;
            continue;
        }

        if (_sensBufIdx < totalLen) continue;

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

void uart_sensor_send_control(uint8_t fan_level) {
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
