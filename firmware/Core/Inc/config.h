#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* ============================================================
 * 系统时钟配置
 * ============================================================ */
#define SYSTEM_CLOCK_HZ         250000000UL
#define SYSTICK_PERIOD_MS       1

/* ============================================================
 * 传感器采样参数
 * ============================================================ */
#define SENSOR_SAMPLE_INTERVAL_MS   1000
#define SENSOR_RETRY_MAX            3
#define SENSOR_TIMEOUT_MS           100

/* SHT40 I2C地址与参数 */
#define SHT40_I2C_ADDR          0x44
#define SHT40_I2C_TIMEOUT       50

/* SGP40 I2C地址与参数 */
#define SGP40_I2C_ADDR          0x59
#define SGP40_I2C_TIMEOUT       50

/* PMS7003 UART参数 */
#define PMS7003_BAUDRATE        9600
#define PMS7003_FRAME_LEN       32

/* 霍尔电流传感器参数 */
#define HALL_ADC_MAX            4095
#define HALL_VREF               3.3f
#define HALL_SENSITIVITY        0.185f     /* 185mV/A */
#define HALL_OFFSET_V           1.65f      /* Vcc/2 零电流输出 */

/* ============================================================
 * AI模型参数
 * ============================================================ */
#define AI_MODEL_INPUT_DIM      8
#define AI_MODEL_SEQ_LEN        16
#define AI_MODEL_INPUT_SIZE     (AI_MODEL_SEQ_LEN * AI_MODEL_INPUT_DIM)
#define AI_MODEL_OUTPUT_DIM     6          /* 风机档位 0-5 */
#define AI_MODEL_AUX_OUTPUT_DIM 4          /* 空气质量类别 */
#define AI_INFERENCE_INTERVAL_MS 1000

/* ============================================================
 * 电机控制参数
 * ============================================================ */
#define MOTOR_PWM_FREQ_HZ       20000
#define MOTOR_PWM_MAX_DUTY      1000       /* TIM ARR */
#define MOTOR_MAX_CURRENT_A     8.0f
#define MOTOR_OVERLOAD_CURRENT_A 7.5f
#define MOTOR_STARTUP_RAMP_MS   500

/* 风机档位对应占空比 */
#define FAN_LEVEL_0             0
#define FAN_LEVEL_1             300
#define FAN_LEVEL_2             450
#define FAN_LEVEL_3             600
#define FAN_LEVEL_4             800
#define FAN_LEVEL_5             1000

extern const uint16_t FAN_DUTY_TABLE[6];

/* ============================================================
 * OpenMV通信参数
 * ============================================================ */
#define OPENMV_UART_BAUDRATE    921600
#define OPENMV_FRAME_HEADER     0xAA55
#define OPENMV_FRAME_MAX_LEN    256

/* ============================================================
 * 系统控制参数
 * ============================================================ */
#define PERSON_ABSENT_TIMEOUT_S 180        /* 无人3分钟后待机 */
#define COOLDOWN_TIMEOUT_S      60
#define FILTER_LIFE_MAX_HOURS   2000       /* 滤网寿命2000小时 */

/* ============================================================
 * 调试开关
 * ============================================================ */
#define DEBUG_ENABLE            1
#if DEBUG_ENABLE
  #define DEBUG_UART            USART2
  #define DEBUG_BAUDRATE        115200
#endif

#endif /* CONFIG_H */
