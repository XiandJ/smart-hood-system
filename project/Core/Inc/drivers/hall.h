#ifndef __HALL_H
#define __HALL_H

#include <stdint.h>

int8_t  Hall_Init(void);
float   Hall_GetCurrent(void);
float   Hall_GetInstantCurrent(void);
void    Hall_SetWindowSize(uint16_t samples);
uint8_t Hall_IsOvercurrent(float threshold);

#endif /* __HALL_H */
