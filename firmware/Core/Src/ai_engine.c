#include "ai_engine.h"
#include "stm32h5xx_hal.h"
#include <string.h>
#include <math.h>

/* ============================================================
 * AI模型占位符
 * 实际部署时由STM32Cube.AI生成的model.c替换
 * 包含: model_init(), model_infer(), model_input/output tensor pointers
 * ============================================================ */
#ifdef AI_MODEL_ENABLED
  #include "model.h"  /* STM32Cube.AI生成的头文件 */
#endif

AIEngine_t g_ai_engine;

/* 外部声明 */
extern SensorHub_t g_sensor_hub;

/* 档位Duty表 */
const uint16_t FAN_DUTY_TABLE[6] = {
    FAN_LEVEL_0, FAN_LEVEL_1, FAN_LEVEL_2,
    FAN_LEVEL_3, FAN_LEVEL_4, FAN_LEVEL_5
};

/* ============================================================
 * 特征工程 - 归一化参数
 * ============================================================ */

/* 各特征的均值和标准差 (训练时计算，部署时硬编码) */
typedef struct {
    float mean;
    float std;
} FeatureNorm_t;

static const FeatureNorm_t feat_norm[AI_MODEL_INPUT_DIM] = {
    { 25.0f, 15.0f },    /* 温度: 均值25°C, std 15 */
    { 55.0f, 25.0f },    /* 湿度: 均值55%, std 25 */
    { 100.0f, 80.0f },   /* VOC指数: 均值100, std 80 */
    { 30.0f, 40.0f },    /* PM1.0: 均值30μg/m³, std 40 */
    { 50.0f, 60.0f },    /* PM2.5: 均值50μg/m³, std 60 */
    { 60.0f, 70.0f },    /* PM10: 均值60μg/m³, std 70 */
    { 1.5f, 1.5f },      /* 电流: 均值1.5A, std 1.5 */
    { 0.0f, 0.5f },      /* di/dt: 均值0, std 0.5 */
};

/* ============================================================
 * 滑动窗口管理
 * ============================================================ */

static void FeatureWindow_Push(FeatureWindow_t* fw, const sensor_fusion_t* fusion) {
    memcpy(fw->buffer[fw->write_idx], fusion->features,
           AI_MODEL_INPUT_DIM * sizeof(float));
    fw->write_idx = (fw->write_idx + 1) % AI_MODEL_SEQ_LEN;
    if (fw->write_idx == 0) fw->is_full = 1;
}

static void FeatureWindow_Flatten(const FeatureWindow_t* fw, float* flat) {
    /* 将滑动窗口展开并按时间顺序排列 */
    uint8_t start = fw->is_full ? fw->write_idx : 0;
    for (uint8_t t = 0; t < AI_MODEL_SEQ_LEN; t++) {
        uint8_t idx = (start + t) % AI_MODEL_SEQ_LEN;
        /* 未填满的部分用零填充 */
        if (!fw->is_full && t >= fw->write_idx) {
            memset(&flat[t * AI_MODEL_INPUT_DIM], 0,
                   AI_MODEL_INPUT_DIM * sizeof(float));
        } else {
            memcpy(&flat[t * AI_MODEL_INPUT_DIM], fw->buffer[idx],
                   AI_MODEL_INPUT_DIM * sizeof(float));
        }
    }
}

/* Z-Score归一化 + int8量化 */
static void FeatureWindow_Quantize(const float* flat, int8_t* quantized) {
    for (uint16_t i = 0; i < AI_MODEL_INPUT_SIZE; i++) {
        uint8_t feat_idx = i % AI_MODEL_INPUT_DIM;
        float norm_val = (flat[i] - feat_norm[feat_idx].mean) / feat_norm[feat_idx].std;
        /* 裁剪到 [-4, 4] 标准差范围 */
        if (norm_val > 4.0f)  norm_val = 4.0f;
        if (norm_val < -4.0f) norm_val = -4.0f;
        /* int8量化: scale = 0.03125 (1/32) */
        int32_t q_val = (int32_t)(norm_val * 32.0f);
        if (q_val > 127)  q_val = 127;
        if (q_val < -128) q_val = -128;
        quantized[i] = (int8_t)q_val;
    }
}

/* ============================================================
 * AI引擎初始化
 * ============================================================ */

uint8_t AIEngine_Init(void) {
    memset(&g_ai_engine, 0, sizeof(g_ai_engine));

#ifdef AI_MODEL_ENABLED
    if (model_init() != 0) {
        g_ai_engine.model_loaded = 0;
        return 0;
    }
    g_ai_engine.model_loaded = 1;
#else
    g_ai_engine.model_loaded = 0;
#endif
    return 1;
}

void AIEngine_FeedSensorData(const sensor_fusion_t* fusion) {
    FeatureWindow_Push(&g_ai_engine.window, fusion);
}

/* ============================================================
 * TFLite Micro推理 (模型可用时)
 * ============================================================ */

#ifdef AI_MODEL_ENABLED
static uint8_t AIEngine_Infer_TFLite(void) {
    float flat_input[AI_MODEL_INPUT_SIZE];
    uint32_t t_start, t_end;

    if (!g_ai_engine.window.is_full) return 0;

    /* 展开窗口 → 归一化 → int8量化 */
    FeatureWindow_Flatten(&g_ai_engine.window, flat_input);
    FeatureWindow_Quantize(flat_input, g_ai_engine.model_input);

    /* 将量化输入拷贝到模型输入tensor */
    memcpy(model_input_tensor(), g_ai_engine.model_input, AI_MODEL_INPUT_SIZE);

    /* 执行推理 */
    t_start = DWT->CYCCNT;
    model_infer();
    t_end = DWT->CYCCNT;
    g_ai_engine.result.inference_time_us = (t_end - t_start) / (SystemCoreClock / 1000000);

    /* 解析输出 */
    const int8_t* output = model_output_tensor();
    const int8_t* aux_output = model_aux_output_tensor();

    /* 主输出: 风机档位 Softmax(int8) → float */
    float softmax_sum = 0.0f;
    float temp[6];
    for (uint8_t i = 0; i < AI_MODEL_OUTPUT_DIM; i++) {
        temp[i] = expf((float)output[i] * 0.0625f); /* scale 1/16 */
        softmax_sum += temp[i];
    }

    uint8_t best_level = 0;
    float best_prob = 0.0f;
    for (uint8_t i = 0; i < AI_MODEL_OUTPUT_DIM; i++) {
        g_ai_engine.result.fan_probs[i] = temp[i] / softmax_sum;
        if (g_ai_engine.result.fan_probs[i] > best_prob) {
            best_prob = g_ai_engine.result.fan_probs[i];
            best_level = i;
        }
    }
    g_ai_engine.result.fan_level = best_level;

    /* 辅助输出: 空气质量类别 */
    float aq_best = 0.0f;
    for (uint8_t i = 0; i < AI_MODEL_AUX_OUTPUT_DIM; i++) {
        g_ai_engine.result.aq_probs[i] = (float)aux_output[i] * 0.0078125f; /* scale 1/128 */
        if (g_ai_engine.result.aq_probs[i] > aq_best) {
            aq_best = g_ai_engine.result.aq_probs[i];
            g_ai_engine.result.air_quality_class = i;
        }
    }

    g_ai_engine.result.anomaly_score = 0.0f;
    return 1;
}
#endif

/* ============================================================
 * 规则引擎回退 (AI模型不可用时)
 *
 * 基于多阈值+加权评分的规则系统
 * PM2.5权重最高(油烟主要指标), VOC次之, 温湿度辅助
 * ============================================================ */

AIResult_t AIEngine_RuleBasedFallback(const sensor_fusion_t* fusion) {
    AIResult_t result;
    memset(&result, 0, sizeof(result));

    const float* f = fusion->features;
    float pm25  = f[4];  /* PM2.5 */
    float pm10  = f[5];  /* PM10 */
    float voc   = f[2];  /* VOC指数 */
    float temp  = f[0];  /* 温度 */
    float rh    = f[1];  /* 湿度 */
    float i_m   = f[6];  /* 电流 */
    float di_dt = f[7];  /* 电流变化率 */

    /* 综合污染指数 (0-100) */
    float pollution_score = 0.0f;

    /* PM2.5评分 (权重50%) */
    if (pm25 < 35)      pollution_score += pm25 * 0.714f;        /* 0-25 */
    else if (pm25 < 75) pollution_score += 25.0f + (pm25 - 35) * 0.625f;  /* 25-50 */
    else if (pm25 < 150) pollution_score += 50.0f + (pm25 - 75) * 0.333f;  /* 50-75 */
    else                pollution_score += 75.0f;                /* >75 */

    /* PM10贡献 (权重20%) */
    if (pm10 < 50)      pollution_score += pm10 * 0.2f;          /* 0-10 */
    else if (pm10 < 150) pollution_score += 10.0f + (pm10 - 50) * 0.1f;  /* 10-20 */
    else                pollution_score += 20.0f;                /* >20 */

    /* VOC贡献 (权重20%) */
    if (voc < 80)       pollution_score += voc * 0.125f;         /* 0-10 */
    else if (voc < 300) pollution_score += 10.0f + (voc - 80) * 0.045f; /* 10-20 */
    else                pollution_score += 20.0f;

    /* 温度异常贡献 (权重5%) */
    if (temp > 45.0f)   pollution_score += 5.0f * (temp - 45.0f) / 15.0f;

    /* 高湿度贡献 (权重5%) */
    if (rh > 80.0f)     pollution_score += 5.0f * (rh - 80.0f) / 20.0f;

    if (pollution_score > 100.0f) pollution_score = 100.0f;
    if (pollution_score < 0.0f)   pollution_score = 0.0f;

    /* 分数 → 档位映射 */
    if (pollution_score < 5.0f)       result.fan_level = 0;
    else if (pollution_score < 20.0f) result.fan_level = 1;
    else if (pollution_score < 40.0f) result.fan_level = 2;
    else if (pollution_score < 65.0f) result.fan_level = 3;
    else if (pollution_score < 85.0f) result.fan_level = 4;
    else                              result.fan_level = 5;

    for (uint8_t i = 0; i < 6; i++) {
        result.fan_probs[i] = (i == result.fan_level) ? 1.0f : 0.0f;
    }

    /* 空气质量分类 */
    if (pollution_score < 20.0f)      result.air_quality_class = AQ_GOOD;
    else if (pollution_score < 50.0f) result.air_quality_class = AQ_MODERATE;
    else if (pollution_score < 80.0f) result.air_quality_class = AQ_POOR;
    else                              result.air_quality_class = AQ_HAZARDOUS;

    /* 异常检测 - 基于di/dt和电流 */
    if (fabsf(di_dt) > 3.0f || i_m > MOTOR_OVERLOAD_CURRENT_A) {
        result.anomaly_score = 0.8f;
    } else if (fabsf(di_dt) > 1.5f) {
        result.anomaly_score = 0.4f;
    } else {
        result.anomaly_score = 0.0f;
    }

    result.inference_time_us = 0;
    return result;
}

/* ============================================================
 * 主推理入口
 * ============================================================ */

uint8_t AIEngine_Infer(void) {
#ifdef AI_MODEL_ENABLED
    if (g_ai_engine.model_loaded) {
        return AIEngine_Infer_TFLite();
    }
#endif
    /* 回退到规则引擎 */
    if (!g_ai_engine.window.is_full) return 0;

    float flat[AI_MODEL_INPUT_SIZE];
    FeatureWindow_Flatten(&g_ai_engine.window, flat);

    sensor_fusion_t latest;
    memcpy(latest.features,
           g_ai_engine.window.buffer[(g_ai_engine.window.write_idx - 1) % AI_MODEL_SEQ_LEN],
           AI_MODEL_INPUT_DIM * sizeof(float));

    g_ai_engine.result = AIEngine_RuleBasedFallback(&latest);
    return 1;
}

const AIResult_t* AIEngine_GetResult(void) {
    return &g_ai_engine.result;
}
