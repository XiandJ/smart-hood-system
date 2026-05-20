#include "system_manager.h"
#include "stm32h5xx_hal.h"

SystemContext_t   g_sys;
DecisionContext_t g_decision;

/* 前向声明 */
static void SM_UpdateUptime(void);
static void SM_UpdatePersonAbsent(void);
static void SM_GestureToCommand(void);

void SystemManager_Init(void) {
    memset(&g_sys, 0, sizeof(g_sys));
    memset(&g_decision, 0, sizeof(g_decision));

    g_sys.state = SYS_OFF;
    g_sys.mode  = MODE_AUTO;

    /* 初始化各子系统 */
    SensorHub_Init();
    AIEngine_Init();
    Motor_Init();
    OpenMV_Init();

    /* 启用OpenMV人员检测 */
    OpenMV_SetMode(OMV_MODE_ALL);

    /* 进入待机 */
    g_sys.state = SYS_STANDBY;
}

void SystemManager_Run(void) {
    static uint32_t last_run = 0;
    uint32_t now = HAL_GetTick();

    if (now - last_run < SM_TASK_PERIOD_MS) return;
    last_run = now;

    SM_UpdateUptime();

    /* 状态机主循环 */
    switch (g_sys.state) {
    case SYS_OFF:       SM_HandleOff();       break;
    case SYS_STANDBY:   SM_HandleStandby();   break;
    case SYS_RUNNING:   SM_HandleRunning();   break;
    case SYS_COOLDOWN:  SM_HandleCooldown();  break;
    case SYS_ALERT:     SM_HandleAlert();     break;
    }

    /* 电机状态更新 (每周期) */
    Motor_Update();
}

/* ============================================================
 * 状态处理
 * ============================================================ */

void SM_HandleOff(void) {
    /* 按下电源键 → 待机 */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET) {
        HAL_Delay(50); /* 消抖 */
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET) {
            g_sys.state = SYS_STANDBY;
            /* 等待按键释放 */
            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET);
        }
    }
}

void SM_HandleStandby(void) {
    /* 传感器采样 (低频, 2秒一次) */
    static uint32_t last_sample = 0;
    if (HAL_GetTick() - last_sample > 2000) {
        SensorHub_SampleAll();
        last_sample = HAL_GetTick();
    }

    /* OpenMV人员检测 → 有人则唤醒 */
    if (g_openmv.rx_frame.person.person_detected) {
        g_sys.person_present = 1;
        g_sys.person_absent_s = 0;
        /* 有人+污染检测 → 自动启动 */
    }

    /* 手动按键启动 */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) {
        HAL_Delay(50);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) {
            g_sys.state = SYS_RUNNING;
            g_sys.mode = MODE_MANUAL;
            g_sys.fan_level = 2; /* 默认2档 */
            Motor_SetLevel(2);
            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET);
        }
    }

    /* 长时间无人 → 关机 */
    if (g_sys.person_absent_s > PERSON_ABSENT_TIMEOUT_S) {
        g_sys.state = SYS_OFF;
    }

    /* 持续污染检测 → 自动启动 */
    if (g_sensor_hub.pms7003.pm2_5 > 75 ||
        g_sensor_hub.sgp40.voc_index > 200) {
        g_sys.state = SYS_RUNNING;
        g_sys.mode = MODE_AUTO;
        SM_SensorFusionDecision();
    }
}

void SM_HandleRunning(void) {
    /* 1. 传感器采样 */
    SensorHub_SampleAll();
    SensorHub_PrintDebug();

    /* 2. 馈入AI引擎 */
    AIEngine_FeedSensorData(&g_sensor_hub.fusion);

    /* 3. 执行AI推理 */
    AIEngine_Infer();

    /* 4. 获取推理结果 */
    g_decision.local_ai = *AIEngine_GetResult();
    g_decision.local_ai_valid = 1;

    /* 5. 获取OpenMV视觉数据 */
    g_decision.vision_smoke   = g_openmv.rx_frame.smoke;
    g_decision.vision_person  = g_openmv.rx_frame.person;
    g_decision.vision_gesture = g_openmv.rx_frame.gesture;
    g_decision.vision_valid   = (g_openmv.rx_frame.crc_ok &&
                                 (HAL_GetTick() - g_openmv.last_rx_tick) < 500);

    /* 6. 传感器融合决策 */
    SM_SensorFusionDecision();

    /* 7. 更新风机 */
    if (g_sys.mode == MODE_AUTO || g_sys.mode == MODE_GESTURE) {
        SM_UpdateFanLevel();
    } else {
        /* 手动模式: 按键控制 */
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET) {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET) {
                if (g_sys.fan_level < 5) {
                    g_sys.fan_level++;
                }
                while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET);
            }
        }
    }

    /* 8. 手势控制 */
    if (g_sys.mode == MODE_GESTURE) {
        SM_GestureToCommand();
    }

    /* 9. 模式切换 (长按模式键) */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) {
        HAL_Delay(50);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) {
            g_sys.mode = (g_sys.mode + 1) % 3;
            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET);
        }
    }

    /* 10. 电机实时电流监控 */
    Motor_UpdateCurrent(g_sensor_hub.hall.current_a);

    /* 11. 人员检测更新 */
    SM_UpdatePersonAbsent();

    /* 12. 异常检测 */
    SM_CheckAlerts();

    /* 13. 滤网寿命更新 */
    SM_UpdateFilterLife();

    /* 14. LED指示更新 */
    SM_UpdateLEDs();

    /* 15. 待机条件: 无人+污染消失30秒 */
    if (g_sys.person_absent_s > 30 &&
        g_sensor_hub.pms7003.pm2_5 < 35 &&
        g_sensor_hub.sgp40.voc_index < 80) {
        g_sys.state = SYS_STANDBY;
        Motor_SetLevel(0);
    }
}

void SM_HandleCooldown(void) {
    static uint32_t cooldown_start = 0;

    if (cooldown_start == 0) {
        cooldown_start = HAL_GetTick();
        Motor_SetLevel(0);
    }

    /* 电机停止+延时 */
    if (g_motor.state == MOTOR_OFF &&
        (HAL_GetTick() - cooldown_start) > COOLDOWN_TIMEOUT_S * 1000) {
        g_sys.state = SYS_STANDBY;
        cooldown_start = 0;
    }
}

void SM_HandleAlert(void) {
    Motor_EmergencyStop();
    /* 报警指示 - LED闪烁 */
    /* 故障清除后手动复位到待机 */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET) {
        HAL_Delay(1000);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET) {
            g_sys.state = SYS_STANDBY;
            g_sys.alert_code = 0;
        }
    }
}

/* ============================================================
 * 决策融合
 * ============================================================ */

void SM_SensorFusionDecision(void) {
    /* 混合决策: AI引擎(传感器融合) + OpenMV视觉(烟雾检测) */

    uint8_t sensor_fan = g_decision.local_ai.fan_level;
    AirQuality_t sensor_aq = (AirQuality_t)g_decision.local_ai.air_quality_class;

    /* 视觉烟雾提升 */
    uint8_t vision_boost = 0;
    if (g_decision.vision_valid && g_decision.vision_smoke.smoke_detected) {
        uint8_t smoke_lvl = g_decision.vision_smoke.smoke_level;
        if (smoke_lvl > 70)       vision_boost = 2;
        else if (smoke_lvl > 30)  vision_boost = 1;
    }

    /* 综合决策 */
    uint8_t fused_fan = sensor_fan;
    if (vision_boost > 0 && fused_fan < sensor_fan + vision_boost) {
        fused_fan = sensor_fan + vision_boost;
    }

    /* OpenMV烟雾也可独立触发 */
    if (g_decision.vision_smoke.smoke_level > 80 && fused_fan < 4) {
        fused_fan = 4;
    }

    if (fused_fan > 5) fused_fan = 5;

    g_decision.fused_fan_level = fused_fan;
    g_decision.fused_air_quality = sensor_aq;
    g_decision.decision_source = (vision_boost > 0) ? 3 : 1;

    /* 异常检测覆盖 */
    if (g_decision.local_ai.anomaly_score > 0.7f) {
        g_decision.decision_source = 0; /* fallback to rule-based safety */
        if (fused_fan > 3) fused_fan = 3; /* 限制最大档位 */
    }
}

void SM_UpdateFanLevel(void) {
    if (g_decision.fused_fan_level != g_sys.fan_level) {
        g_sys.fan_level = g_decision.fused_fan_level;
        Motor_SetLevel(g_sys.fan_level);
    }
}

void SM_UpdateFilterLife(void) {
    if (g_motor.state == MOTOR_RUNNING) {
        g_sys.filter_runtime_s++;
    }

    float used_pct = (float)g_sys.filter_runtime_s /
                     (float)(FILTER_LIFE_MAX_HOURS * 3600) * 100.0f;
    g_sys.filter_life_pct = (used_pct < 100.0f) ? (100.0f - used_pct) : 0.0f;
}

void SM_CheckAlerts(void) {
    /* 传感器全部失效 */
    if (!g_sensor_hub.all_ok) {
        uint8_t fail_count = 0;
        if (g_sensor_hub.sht40_status   != SENSOR_OK) fail_count++;
        if (g_sensor_hub.sgp40_status   != SENSOR_OK) fail_count++;
        if (g_sensor_hub.pms7003_status != SENSOR_OK) fail_count++;
        if (fail_count >= 2) {
            g_sys.state = SYS_ALERT;
            g_sys.alert_code = 0x10;
        }
    }

    /* 电机故障 */
    if (g_motor.state == MOTOR_FAULT) {
        g_sys.state = SYS_ALERT;
        g_sys.alert_code = 0x20 | g_motor.fault_code;
    }

    /* 电机过载 */
    if (Motor_CheckOverload() && g_sys.state == SYS_RUNNING) {
        Motor_SetLevel(g_sys.fan_level > 1 ? g_sys.fan_level - 1 : 0);
    }
}

static void SM_UpdateUptime(void) {
    static uint32_t last_sec = 0;
    uint32_t now = HAL_GetTick();

    if (now - last_sec >= 1000) {
        last_sec = now;
        g_sys.uptime_s++;

        if (g_motor.state == MOTOR_RUNNING) {
            g_motor.runtime_s++;
        }
    }
}

static void SM_UpdatePersonAbsent(void) {
    static uint32_t last_check = 0;

    if (HAL_GetTick() - last_check < 1000) return;
    last_check = HAL_GetTick();

    if (g_decision.vision_valid &&
        g_decision.vision_person.person_detected) {
        g_sys.person_present = 1;
        g_sys.person_absent_s = 0;
    } else {
        if (g_sys.person_present) {
            g_sys.person_absent_s++;
        }
    }
}

static void SM_GestureToCommand(void) {
    if (!g_decision.vision_valid) return;

    uint8_t gesture = g_decision.vision_gesture.gesture_id;
    uint8_t confidence = g_decision.vision_gesture.confidence;

    if (confidence < 70) return; /* 低置信度忽略 */

    switch (gesture) {
    case 1: /* 上滑 → 档位+1 */
        if (g_sys.fan_level < 5) {
            g_sys.fan_level++;
            Motor_SetLevel(g_sys.fan_level);
        }
        break;
    case 2: /* 下滑 → 档位-1 */
        if (g_sys.fan_level > 0) {
            g_sys.fan_level--;
            Motor_SetLevel(g_sys.fan_level);
        }
        break;
    case 3: /* 左滑 → 切换模式 */
        g_sys.mode = (g_sys.mode + 1) % 3;
        break;
    case 4: /* 右滑 → 开关机 */
        if (g_sys.state == SYS_RUNNING) {
            g_sys.state = SYS_COOLDOWN;
        }
        break;
    default:
        break;
    }
}

void SM_UpdateLEDs(void) {
    /* WS2812B 空气质量指示 */
    /* 绿灯=优良, 黄灯=中等, 红灯=差, 红灯闪烁=危害 */

    uint8_t r = 0, g = 0, b = 0;
    switch (g_decision.fused_air_quality) {
    case AQ_GOOD:       g = 64;  break;
    case AQ_MODERATE:   r = 64;  g = 64; break;
    case AQ_POOR:       r = 64;  break;
    case AQ_HAZARDOUS:  r = 128; break;
    }

    /* WS2812B bit-bang (简化处理，实际需SPI/DMA/TIM实现)
     * 这里调用硬件相关的Write函数 */
    /* WS2812_Write(r, g, b); */
    (void)r; (void)g; (void)b; /* 抑制未使用警告 */
}
