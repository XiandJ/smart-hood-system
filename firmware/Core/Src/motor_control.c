#include "motor_control.h"
#include "stm32h5xx_hal.h"
#include "hall.h"

extern TIM_HandleTypeDef htim1;
extern MotorController_t g_motor;

/* 档位对应PWM占空比 */
const uint16_t FAN_DUTY_TABLE[6] = {
    FAN_LEVEL_0, FAN_LEVEL_1, FAN_LEVEL_2,
    FAN_LEVEL_3, FAN_LEVEL_4, FAN_LEVEL_5
};

void Motor_Init(void) {
    g_motor.state = MOTOR_OFF;
    g_motor.current_level = 0;
    g_motor.target_level = 0;
    g_motor.current_duty = 0;
    g_motor.target_duty = 0;
    g_motor.current_a = 0.0f;
    g_motor.power_w = 0.0f;
    g_motor.runtime_s = 0;
    g_motor.overload = 0;
    g_motor.fault_code = 0;

    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
}

void Motor_SetLevel(uint8_t level) {
    if (level > 5) level = 5;
    g_motor.target_level = level;
    g_motor.target_duty = Motor_LevelToDuty(level);
}

void Motor_SetDuty(uint16_t duty) {
    if (duty > MOTOR_PWM_MAX_DUTY) duty = MOTOR_PWM_MAX_DUTY;
    g_motor.target_duty = duty;
}

/* 每10ms调用，实现软启动和斜坡控制 */
void Motor_Update(void) {
    switch (g_motor.state) {
    case MOTOR_OFF:
        if (g_motor.target_duty > 0) {
            g_motor.state = MOTOR_STARTING;
            g_motor.current_duty = 0;
        }
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
        break;

    case MOTOR_STARTING: {
        /* 斜坡启动: 每10ms增加10步直到目标 */
        if (g_motor.current_duty < g_motor.target_duty) {
            g_motor.current_duty += 10;
            if (g_motor.current_duty > MOTOR_PWM_MAX_DUTY) {
                g_motor.current_duty = MOTOR_PWM_MAX_DUTY;
            }
        } else {
            g_motor.state = MOTOR_RUNNING;
            g_motor.current_duty = g_motor.target_duty;
        }
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, g_motor.current_duty);
        break;
    }

    case MOTOR_RUNNING:
        /* 平滑调整 */
        if (g_motor.current_duty < g_motor.target_duty) {
            g_motor.current_duty += 5;
        } else if (g_motor.current_duty > g_motor.target_duty) {
            if (g_motor.current_duty > 5) {
                g_motor.current_duty -= 5;
            } else {
                g_motor.current_duty = 0;
                g_motor.state = MOTOR_OFF;
            }
        }
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, g_motor.current_duty);

        /* 检测是否应停止 */
        if (g_motor.target_duty == 0 && g_motor.current_duty == 0) {
            g_motor.state = MOTOR_OFF;
        }

        /* 运行时过载检测 */
        if (g_motor.overload) {
            Motor_EmergencyStop();
        }
        break;

    case MOTOR_FAULT:
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
        /* 需手动复位 */
        break;
    }
}

void Motor_EmergencyStop(void) {
    g_motor.state = MOTOR_FAULT;
    g_motor.fault_code = 0x01; /* 过载故障 */
    g_motor.target_duty = 0;
    g_motor.current_duty = 0;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET); /* Motor Enable OFF */
}

void Motor_UpdateCurrent(float current_a) {
    g_motor.current_a = current_a;
    g_motor.power_w = current_a * 24.0f;

    if (current_a > MOTOR_MAX_CURRENT_A) {
        /* 硬件过流，紧急停止 */
        g_motor.fault_code = 0x02;
        Motor_EmergencyStop();
    } else if (current_a > MOTOR_OVERLOAD_CURRENT_A) {
        g_motor.overload = 1;
    } else {
        g_motor.overload = 0;
    }
}

uint8_t Motor_CheckOverload(void) {
    return g_motor.overload;
}
