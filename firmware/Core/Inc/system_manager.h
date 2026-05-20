#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include "main.h"
#include "sensor_hub.h"
#include "ai_engine.h"
#include "motor_control.h"
#include "openmv_bridge.h"

/* 系统管理任务周期 */
#define SM_TASK_PERIOD_MS       100

/* 决策上下文 */
typedef struct {
    /* 传感器融合结果 (本地) */
    AIResult_t      local_ai;
    uint8_t         local_ai_valid;

    /* 视觉检测结果 (OpenMV) */
    OMV_SmokeData_t   vision_smoke;
    OMV_PersonData_t  vision_person;
    OMV_GestureData_t vision_gesture;
    uint8_t           vision_valid;

    /* 融合决策 */
    uint8_t         fused_fan_level;    /* 最终风机档位 */
    AirQuality_t    fused_air_quality;
    uint8_t         decision_source;    /* 0=rule, 1=ai, 2=vision, 3=hybrid */
} DecisionContext_t;

extern DecisionContext_t g_decision;

void SystemManager_Init(void);
void SystemManager_Run(void);

/* 内部状态处理 */
void SM_HandleOff(void);
void SM_HandleStandby(void);
void SM_HandleRunning(void);
void SM_HandleCooldown(void);
void SM_HandleAlert(void);

/* 决策与融合 */
void SM_SensorFusionDecision(void);
void SM_UpdateFanLevel(void);
void SM_UpdateFilterLife(void);
void SM_CheckAlerts(void);

/* LED指示 */
void SM_UpdateLEDs(void);

#endif /* SYSTEM_MANAGER_H */
