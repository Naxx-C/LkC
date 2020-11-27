#include"nilm_feature.h"
#include"nilm_utils.h"
#include <string.h>
#include<math.h>

void copyNilmFeature(NilmFeature *dst, NilmFeature *src) {
    memcpy(dst, src, sizeof(NilmFeature));
}

/**
 * 标准化 原因：不同特征的量纲差距比较大；部分特征实际是不稳定的，两次结果可能大不相同，
 *
 * @param feature
 * @param normalized
 */
float normalizePowerFactor(float powerFactor) {
    return fabs(powerFactor);
}

// 不稳定特征，分段化
float pulseILevel(float pulseI) {
    float level = pulseI;
    if (pulseI < 1.1f)
        level = 0;
    else if (pulseI >= 1.1f && pulseI < 1.5f)
        level = 1;
    else if (pulseI >= 1.5f && pulseI < 2.0f)
        level = 2;
    else if (pulseI >= 2.0f)
        level = 3;
    return level;
}

float normalizePulseI(float pulseI) {
    return (0.7f + pulseILevel(fabs(pulseI)) / 10); // [0.7,1]
}

// the bigger, the more flatten in high level
#define POWER_NORM_FACTOR 0.5
static const float APP_MAX_POWER = (float) pow(2000, POWER_NORM_FACTOR);
float normalizeActivePower(float activePower) {
    return ((float) (pow(fabs(activePower), POWER_NORM_FACTOR) / APP_MAX_POWER));
}
