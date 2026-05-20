/**
 * AI模型占位头文件
 *
 * 此文件在STM32Cube.AI部署后由 code generator 替换
 * 生成命令:
 *   stm32ai generate -m sensor_fusion_model_quant.tflite -o ./Middleware/AI/
 *
 * 实际生成后，此文件将包含:
 *   - model_init()          初始化函数
 *   - model_infer()         推理函数
 *   - model_input_tensor()  输入张量指针
 *   - model_output_tensor() 主输出张量指针 (风机档位)
 *   - model_aux_output_tensor() 辅助输出张量指针 (空气质量)
 */

#ifndef AI_MODEL_H
#define AI_MODEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 模型输入输出维度 */
#define AI_MODEL_INPUT_SIZE      128   /* 16*8 */
#define AI_MODEL_OUTPUT_SIZE     6
#define AI_MODEL_AUX_OUTPUT_SIZE 4

/* ============================================================
 * 当AI_MODEL_ENABLED未定义时，使用规则引擎回退
 * 以下是占位函数 (ai_engine.c中已包含规则回退逻辑)
 * ============================================================ */

#ifndef AI_MODEL_ENABLED

/* 返回非0值表示模型不可用，触发规则引擎 */
static inline int model_init(void) {
    return -1;
}

static inline int model_infer(void) {
    return -1;
}

static inline int8_t* model_input_tensor(void) {
    return NULL;
}

static inline int8_t* model_output_tensor(void) {
    return NULL;
}

static inline int8_t* model_aux_output_tensor(void) {
    return NULL;
}

#endif /* !AI_MODEL_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* AI_MODEL_H */
