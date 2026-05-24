#ifndef __AI_ENGINE_H
#define __AI_ENGINE_H

#include <stdint.h>
#include "config.h"

void    AI_Init(void);
uint8_t AI_Evaluate(const sensor_data_t *data);

#endif /* __AI_ENGINE_H */
