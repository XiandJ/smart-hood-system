#ifndef AI_ENGINE_H
#define AI_ENGINE_H

#include "config.h"
#include "sensor_hub.h"
#include <stdint.h>

/* AI推理结果 */
typedef struct {
    uint8_t     fan_level;          /* 预测档位 0-5 */
    float       fan_probs[6];       /* 各档位概率 */
    uint8_t     air_quality_class;  /* 0=优,1=良,2=中,3=差 */
    float       aq_probs[4];
    float       anomaly_score;      /* 异常分数 0-1 */
    uint32_t    inference_time_us;  /* 推理耗时 */
} AIResult_t;

/* 特征工程 - 滑动窗口 */
typedef struct {
    float buffer[AI_MODEL_SEQ_LEN][AI_MODEL_INPUT_DIM];
    uint8_t write_idx;
    uint8_t is_full;
} FeatureWindow_t;

/* AI引擎上下文 */
typedef struct {
    FeatureWindow_t window;
    AIResult_t      result;
    int8_t          model_input[AI_MODEL_INPUT_SIZE];    /* int8量化输入 */
    uint8_t         model_loaded;
    uint32_t        last_inference_tick;
} AIEngine_t;

extern AIEngine_t g_ai_engine;

/* 初始化AI引擎 (加载模型) */
uint8_t AIEngine_Init(void);

/* 添加传感器数据到滑动窗口 */
void AIEngine_FeedSensorData(const sensor_fusion_t* fusion);

/* 执行推理 */
uint8_t AIEngine_Infer(void);

/* 获取最新推理结果 */
const AIResult_t* AIEngine_GetResult(void);

/* 简单的规则融合: 当AI不可用时使用 */
AIResult_t AIEngine_RuleBasedFallback(const sensor_fusion_t* fusion);

#endif /* AI_ENGINE_H */
