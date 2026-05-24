#ifndef __PMS7003_H
#define __PMS7003_H

#include <stdint.h>

typedef struct {
    uint16_t pm1_0_cf1;
    uint16_t pm2_5_cf1;
    uint16_t pm10_cf1;
    uint16_t pm1_0_atm;
    uint16_t pm2_5_atm;
    uint16_t pm10_atm;
    uint16_t cnt_0_3um;
    uint16_t cnt_0_5um;
    uint16_t cnt_1_0um;
    uint16_t cnt_2_5um;
    uint16_t cnt_5_0um;
    uint16_t cnt_10_0um;
    uint8_t  version;
    uint8_t  error_code;
    uint8_t  checksum_ok;
} pms7003_data_t;

int8_t  PMS7003_Init(void);
int8_t  PMS7003_GetData(pms7003_data_t *data);
uint8_t PMS7003_IsDataReady(void);

#endif /* __PMS7003_H */
