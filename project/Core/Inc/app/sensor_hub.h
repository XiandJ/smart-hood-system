#ifndef __SENSOR_HUB_H
#define __SENSOR_HUB_H

#include "config.h"

int8_t SensorHub_Init(void);
void   SensorHub_Update(sensor_data_t *out);

#endif /* __SENSOR_HUB_H */
