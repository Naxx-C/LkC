#ifndef UTILS_POWER_H_
#define UTILS_POWER_H_

#include <math.h>
/**
 * 有功功率，可正可负
 */
float getActivePower(float cur[], int curStart, float vol[], int volStart, int len);
/**
 * 有效值
 */
float getEffectiveValue(float data[], int start, int len);
// 过零检测
int getZeroCrossIndex(float data[], int start, int end);

/**
 * absThresh:绝对值波动允许范围
 * relativeRatio:相对比例波动允许范围，如1=1%
 */
int isPowerStable(float data[], int len, float absThresh, int relativeRatio);

float getStandardPower(float power, float vol);
float getReactivePower(float activePower, float apparentPower);
#endif
