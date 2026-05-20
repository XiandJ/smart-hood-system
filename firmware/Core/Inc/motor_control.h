#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "config.h"
#include <stdint.h>

typedef enum {
    MOTOR_OFF = 0,
    MOTOR_STARTING,
    MOTOR_RUNNING,
    MOTOR_FAULT
} MotorState_t;

typedef struct {
    MotorState_t state;
    uint8_t      current_level;    /* 当前档位 0-5 */
    uint8_t      target_level;     /* 目标档位 0-5 */
    uint16_t     current_duty;     /* 当前PWM占空比 */
    uint16_t     target_duty;      /* 目标PWM占空比 */
    float        current_a;        /* 当前电流 */
    float        power_w;          /* 当前功率 */
    uint32_t     runtime_s;        /* 电机累计运行时间 */
    uint8_t      overload;         /* 过载标志 */
    uint8_t      fault_code;       /* 故障码 */
} MotorController_t;

extern MotorController_t g_motor;

void Motor_Init(void);
void Motor_SetLevel(uint8_t level);
void Motor_SetDuty(uint16_t duty);
void Motor_Update(void);           /* 每10ms调用，处理斜坡 */
void Motor_EmergencyStop(void);
void Motor_UpdateCurrent(float current_a);
uint8_t Motor_CheckOverload(void);

static inline uint16_t Motor_LevelToDuty(uint8_t level) {
    extern const uint16_t FAN_DUTY_TABLE[6];
    if (level > 5) level = 5;
    return FAN_DUTY_TABLE[level];
}

#endif /* MOTOR_CONTROL_H */
