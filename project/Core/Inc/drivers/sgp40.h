#ifndef __SGP40_H
#define __SGP40_H

#include <stdint.h>

int8_t SGP40_Init(void);
int8_t SGP40_MeasureRaw(float temp_c, float hum_pct, uint16_t *sraw_voc);
int8_t SGP40_HeaterOff(void);

uint16_t SGP40_RawToVOCIndex(uint16_t sraw_voc);

#endif /* __SGP40_H */
