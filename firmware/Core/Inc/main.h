#ifndef MAIN_H
#define MAIN_H

#include "stm32h5xx_hal.h"
#include "config.h"

/* 系统状态枚举 */
typedef enum {
    SYS_OFF = 0,
    SYS_STANDBY,
    SYS_RUNNING,
    SYS_COOLDOWN,
    SYS_ALERT
} SystemState_t;

/* 运行模式 */
typedef enum {
    MODE_AUTO = 0,
    MODE_MANUAL,
    MODE_GESTURE
} OperationMode_t;

/* 空气质量等级 */
typedef enum {
    AQ_GOOD = 0,
    AQ_MODERATE,
    AQ_POOR,
    AQ_HAZARDOUS
} AirQuality_t;

/* 系统全局上下文 */
typedef struct {
    SystemState_t   state;
    OperationMode_t mode;
    uint8_t         fan_level;         /* 0-5 */
    uint8_t         fan_target;        /* AI推理目标值 */
    AirQuality_t    air_quality;
    uint32_t        uptime_s;
    uint32_t        motor_runtime_s;
    uint32_t        filter_runtime_s;
    float           filter_life_pct;   /* 滤网剩余寿命% */
    uint8_t         person_present;
    uint32_t        person_absent_s;
    uint8_t         alert_code;
} SystemContext_t;

extern SystemContext_t g_sys;

void SystemClock_Config(void);
void GPIO_Init(void);
void I2C1_Init(void);
void UART1_Init(void);
void UART3_Init(void);
void ADC1_Init(void);
void TIM1_PWM_Init(void);
void IWDG_Init(void);
void Error_Handler(void);

#endif /* MAIN_H */
