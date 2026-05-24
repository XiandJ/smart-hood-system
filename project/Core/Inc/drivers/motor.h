#ifndef __MOTOR_H
#define __MOTOR_H

#include <stdint.h>

/* Speed levels */
#define MOTOR_SPEED_OFF  0
#define MOTOR_SPEED_1    1   /* 30% */
#define MOTOR_SPEED_2    2   /* 45% */
#define MOTOR_SPEED_3    3   /* 60% */
#define MOTOR_SPEED_4    4   /* 80% */
#define MOTOR_SPEED_5    5   /* 100% */

int8_t  Motor_Init(void);
void    Motor_SetSpeed(uint8_t level);
uint8_t Motor_GetSpeed(void);
void    Motor_EmergencyStop(void);
void    Motor_ClearEmergency(void);
void    Motor_Update(void);

#endif /* __MOTOR_H */
