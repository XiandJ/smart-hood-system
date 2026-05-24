#include "drivers/motor.h"
#include "drivers/hall.h"
#include "config.h"
#include "tim.h"
#include "main.h"

/* TIM1: 250MHz / (PSC=0) / (ARR=12499) = 20kHz */
#define MOTOR_PWM_ARR       12499

/* Ramp: 10 steps per 10ms call → 500ms full range */
#define RAMP_STEP           ((MOTOR_PWM_ARR + 1) / 100)

/* Duty lookup: level → CCR value */
static const uint16_t duty_table[6] = {
    0,                          /* OFF   =  0% */
    (uint16_t)(12500 * 0.30f),  /* 30% */
    (uint16_t)(12500 * 0.45f),  /* 45% */
    (uint16_t)(12500 * 0.60f),  /* 60% */
    (uint16_t)(12500 * 0.80f),  /* 80% */
    12500                       /* 100% */
};

/* Overcurrent thresholds (Amps) */
#define OC_WARN_THRESHOLD   7.5f
#define OC_TRIP_THRESHOLD   8.0f

static volatile uint8_t  target_level  = MOTOR_SPEED_OFF;
static volatile uint16_t current_ccr   = 0;
static volatile uint8_t  emergency     = 0;
static uint8_t           pwm_running   = 0;

/* ---- helpers ---- */

static void motor_set_pwm(uint16_t ccr) {
    if (ccr == 0) {
        if (pwm_running) {
            HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
            pwm_running = 0;
        }
    } else {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr);
        if (!pwm_running) {
            HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
            pwm_running = 1;
        }
    }
}

static void motor_ramp(uint16_t target) {
    if (current_ccr < target) {
        current_ccr += RAMP_STEP;
        if (current_ccr > target) current_ccr = target;
    } else if (current_ccr > target) {
        if (current_ccr < RAMP_STEP + target)
            current_ccr = target;
        else
            current_ccr -= RAMP_STEP;
    }
    motor_set_pwm(current_ccr);
}

static void motor_check_overcurrent(void) {
    float i = Hall_GetInstantCurrent();
    if (i < 0) i = -i;
    if (i > OC_TRIP_THRESHOLD) {
        Motor_EmergencyStop();
    }
}

/* ---- public API ---- */

int8_t Motor_Init(void) {
    HAL_GPIO_WritePin(MOTOR_DIR_GPIO_Port, MOTOR_DIR_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_EN_GPIO_Port,  MOTOR_EN_Pin,  GPIO_PIN_RESET);

    current_ccr  = 0;
    target_level = MOTOR_SPEED_OFF;
    emergency    = 0;
    pwm_running  = 0;

    motor_set_pwm(0);
    return 0;
}

void Motor_SetSpeed(uint8_t level) {
    if (emergency && level > 0) return;   /* need explicit clear */
    if (level > MOTOR_SPEED_5)  level = MOTOR_SPEED_5;
    target_level = level;

    if (level > 0) {
        HAL_GPIO_WritePin(MOTOR_EN_GPIO_Port, MOTOR_EN_Pin, GPIO_PIN_SET);
    }
}

uint8_t Motor_GetSpeed(void) {
    return target_level;
}

void Motor_EmergencyStop(void) {
    motor_set_pwm(0);
    current_ccr  = 0;
    target_level = MOTOR_SPEED_OFF;
    emergency    = 1;
    HAL_GPIO_WritePin(MOTOR_EN_GPIO_Port, MOTOR_EN_Pin, GPIO_PIN_RESET);
}

void Motor_ClearEmergency(void) {
    emergency = 0;
}

void Motor_Update(void) {
    if (emergency) return;

    motor_check_overcurrent();
    if (emergency) return;

    uint16_t target = duty_table[target_level];
    motor_ramp(target);

    if (target_level == MOTOR_SPEED_OFF && current_ccr == 0) {
        HAL_GPIO_WritePin(MOTOR_EN_GPIO_Port, MOTOR_EN_Pin, GPIO_PIN_RESET);
    }
}
