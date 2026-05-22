#ifndef UART_SENSOR_H
#define UART_SENSOR_H

#include <Arduino.h>

struct SensorData {
    float    temperature;
    float    humidity;
    uint16_t voc;
    uint16_t pm25;
    float    current;
    uint8_t  fan_level;
    uint8_t  state;
    uint8_t  ai_result;
    uint32_t last_update;
    bool     valid;
};

extern SensorData g_sensorData;
extern SemaphoreHandle_t g_sensorMutex;

void uart_sensor_begin();
void uart_sensor_update();
void uart_sensor_send_control(uint8_t fan_level);

#endif
