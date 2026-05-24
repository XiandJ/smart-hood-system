#include "system_state.h"
#include "drivers/motor.h"
#include "config.h"
#include "main.h"

/* ── static state ── */

static volatile system_state_t sys_state = SYS_OFF;
static volatile uint8_t ai_motor_level   = 0;

/* ── button debounce (call @ 100Hz) ── */

typedef struct {
    uint8_t samples;
    uint8_t stable;
    uint8_t prev;
    uint8_t pressed;
} btn_t;

static btn_t b_key0, b_key1, b_wkup;

static void btn_update(btn_t *b, uint8_t raw) {
    b->samples = (b->samples << 1) | (raw ? 1 : 0);
    if (b->samples == 0x00) b->stable = 0;
    else if (b->samples == 0xFF) b->stable = 1;
    b->pressed = (b->stable && !b->prev);
    b->prev = b->stable;
}

/* ── buzzer / LED ── */

static uint16_t beep_count = 0;
static uint16_t led_cnt    = 0;
static uint8_t  led_state  = 0;

static void buzzer_beep(uint16_t ticks_10ms) {
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
    beep_count = ticks_10ms;
}

/* ── public ── */

system_state_t SystemManager_GetState(void) {
    return sys_state;
}

void SystemManager_SetAIMotorLevel(uint8_t level) {
    ai_motor_level = level;
}

/* ── main loop (call @ 100Hz) ── */

void SystemManager_Update(void) {
    /* 1. buttons */
    btn_update(&b_key0, HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_RESET);
    btn_update(&b_key1, HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET);
    btn_update(&b_wkup, HAL_GPIO_ReadPin(WK_UP_GPIO_Port, WK_UP_Pin) == GPIO_PIN_SET);

    /* 2. buzzer auto-off */
    if (beep_count > 0 && --beep_count == 0) {
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
    }

    /* 3. KEY0: toggle OFF ↔ AUTO */
    if (b_key0.pressed) {
        if (sys_state == SYS_OFF) {
            Motor_ClearEmergency();
            sys_state = SYS_AUTO;
            buzzer_beep(20);
        } else if (sys_state == SYS_AUTO) {
            Motor_SetSpeed(MOTOR_SPEED_OFF);
            sys_state = SYS_OFF;
            buzzer_beep(10);
        }
    }

    /* 4. KEY1: cycle MANUAL speed */
    if (b_key1.pressed) {
        sys_state = SYS_MANUAL;
        uint8_t s = Motor_GetSpeed();
        s = (s >= MOTOR_SPEED_5) ? MOTOR_SPEED_OFF : s + 1;
        Motor_SetSpeed(s);
        buzzer_beep(5);
    }

    /* 5. WK_UP: → OFF */
    if (b_wkup.pressed) {
        Motor_SetSpeed(MOTOR_SPEED_OFF);
        sys_state = SYS_OFF;
        buzzer_beep(10);
    }

    /* 6. AUTO mode: apply AI motor level */
    if (sys_state == SYS_AUTO) {
        Motor_SetSpeed(ai_motor_level);
    }

    /* 7. LED */
    if (sys_state == SYS_OFF) {
        HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_RESET);
        led_cnt = 0;
    } else if (sys_state == SYS_MANUAL) {
        HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, GPIO_PIN_SET);
        led_cnt = 0;
    } else {
        /* SYS_AUTO: 1Hz blink */
        if (led_cnt > 0) {
            --led_cnt;
        } else {
            led_state ^= 1;
            led_cnt = 50;
            HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin,
                              led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }
    }
}
