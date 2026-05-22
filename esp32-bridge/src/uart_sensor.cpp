#include "uart_sensor.h"

SensorData g_sensorData = {0};
SemaphoreHandle_t g_sensorMutex = xSemaphoreCreateMutex();
