#include <math.h>
/**
 * 有功功率，可正可负
 */
float getActivePower(float cur[], int curStart, float vol[], int volStart, int len) {

    float activePower = 0;
    for (int i = 0; i < len; i++) {
        activePower += cur[curStart + i] * vol[volStart + i];
    }
    activePower /= len;
    return activePower; // > 0 ? activePower : -activePower;
}

/**
 * 有效值
 */
float getEffectiveValue(float data[], int start, int len) {
    float sumOfSquares = 0;
    int end = start + len;
    for (int i = start; i < end; i++)
        sumOfSquares += data[i] * data[i];

    return (float) sqrt(sumOfSquares / len);
}

// 过零检测 [start,end)
int getZeroCrossIndex(float data[], int start, int end) {
    for (int i = start; i < end; i++) {
        if (data[i] <= 0.0f && data[i + 1] > 0.0f) {
            if (i - 1 >= start && i + 2 < end) {
                if (data[i - 1] < 0.0f && data[i + 2] > 0.0f)
                    return i + 1;
            } else {
                return i + 1;
            }
        }
    }
    return -1;
}

/**
 * absThresh:绝对值波动允许范围
 * relativeRatio:相对比例波动允许范围，如1=1%
 */
int isPowerStable(float data[], int len, float absThresh, int relativeRatio) {

    float average = 0, absAverage = 0;
    for (int i = 0; i < len; i++) {
        average += data[i];
    }
    average /= len;
    absAverage = average >= 0 ? average : -average;
    //平均功率小于20w,返回
//    if (absAverage < 20)
//      return 1;
    for (int i = 0; i < len; i++) {
        float absDelta = data[i] - average;
        absDelta = absDelta >= 0 ? absDelta : -absDelta;

        if (absDelta > absThresh && absDelta * 100 > absAverage * relativeRatio) {
            return 0;
        }
    }
    return 1;
}

/**
 * 获取220v下标称的功率
 *vol:当前电压
 *power:当前功率
 */
float getStandardPower(float power, float vol) {
    return power * (220 * 220 / vol / vol);
}

//通过视在功率计算无功功率
float getReactivePower(float activePower, float apparentPower) {
    return sqrtf(apparentPower * apparentPower - activePower * activePower);
}
