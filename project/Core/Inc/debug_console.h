#ifndef __DEBUG_CONSOLE_H
#define __DEBUG_CONSOLE_H

#include <stdio.h>

void DebugConsole_Init(void);
void LogSensorHeader(void);
void LogSensorData(float temp, float hum, uint16_t voc_raw, uint16_t voc_idx,
                   uint16_t pm2_5, uint16_t pm10, float current, float power);

#endif /* __DEBUG_CONSOLE_H */
