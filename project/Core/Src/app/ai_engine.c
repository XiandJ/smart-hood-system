#include "app/ai_engine.h"

/* ── thresholds ── */

#define PM25_LEVEL1    35
#define PM25_LEVEL2    75
#define PM25_LEVEL3   115
#define PM25_LEVEL4   150

#define VOC_LEVEL1    100
#define VOC_LEVEL2    200
#define VOC_LEVEL3    300

#define CURRENT_OVER   5.0f

/* ── rule engine ── */

void AI_Init(void) {
    /* placeholder for future TFLite model init */
}

uint8_t AI_Evaluate(const sensor_data_t *d) {
    uint8_t pm_level = 0;
    uint8_t voc_level = 0;

    /* PM2.5 rules (priority: highest) */
    if      (d->pm2_5 >= PM25_LEVEL4) pm_level = 5;
    else if (d->pm2_5 >= PM25_LEVEL3) pm_level = 4;
    else if (d->pm2_5 >= PM25_LEVEL2) pm_level = 3;
    else if (d->pm2_5 >= PM25_LEVEL1) pm_level = 2;
    else if (d->pm2_5 > 0)            pm_level = 1;

    /* VOC rules */
    if      (d->voc_index >= VOC_LEVEL3) voc_level = 4;
    else if (d->voc_index >= VOC_LEVEL2) voc_level = 3;
    else if (d->voc_index >= VOC_LEVEL1) voc_level = 2;
    else if (d->voc_index > 0)           voc_level = 1;

    /* take the higher recommendation */
    uint8_t rec = (pm_level > voc_level) ? pm_level : voc_level;

    /* overcurrent: override to max */
    float abs_i = d->current;
    if (abs_i < 0) abs_i = -abs_i;
    if (abs_i > CURRENT_OVER) rec = 5;

    if (rec > 5) rec = 5;
    return rec;
}
