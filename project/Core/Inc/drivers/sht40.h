#ifndef __SHT40_H
#define __SHT40_H

#include <stdint.h>

int8_t SHT40_Init(void);
int8_t SHT40_Read(float *temp_c, float *hum_pct);
int8_t SHT40_GetSerial(uint32_t *serial);

#endif /* __SHT40_H */
