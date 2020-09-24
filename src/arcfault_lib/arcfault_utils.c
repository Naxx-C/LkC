#include "arcfault_utils.h"
#include <stdio.h>
#ifdef ARM_MATH_CM4
#include "arm_math.h"
#else
#include <math.h>
#endif

// 极值点在查找范围内，则认为是吸尘器等造成的电弧误报
static int SEARCH_TOLERANCE = 6;

float arcuMean(float *inputs, int len) {
    float sum = 0;
    for (int i = 0; i < len; i++)
        sum += inputs[i];

    return sum / len;
}

float arcuEffectiveValue(float *inputs, int len) {

#ifdef ARM_MATH_CM4
    float eff = 0;
    arm_rms_f32(inputs, len, &eff);
    return eff;
#else
    float sumOfSquares = 0;
    for (int i = 0; i < len; i++)
        sumOfSquares += inputs[i] * inputs[i];
    return (float) sqrt(sumOfSquares / len);
#endif
}

float arcuThreshAverage(float *inputs, int len, float thresh) {

    float sum = 0;
    int counter = 0;
    for (int i = 0; i < len; i++) {
        if (inputs[i] > thresh) {
            sum += inputs[i];
            counter++;
        }
    }
    if (counter == 0)
        return 0;
    return sum / counter;
}

/**
 * check direction consistent
 *
 * @param current
 * @param float
 *            >0 increase; <0 decrease
 * @param thresh
 *            in case of some small disturb
 * @param startIndex
 * @param checkNum
 *            the points num (start point included) need to be checked, if
 *            no enough points return 0
 * @return return 1 if the direction of following currents is same.
 */
// float* x = { 0, -11, -11.5, -12, -10.5, -14 };
// // float* x = { 0, 11, 11.5, 12, 10.5, 14 };
// log(isConsistent(x, -1, 1.5, 1, 5));
char arcuIsConsistent(float *current, int length, float direction, float thresh, int startIndex, int checkNum) {

    if (startIndex + checkNum > length)
        return 0;
    int endIndex = startIndex + checkNum - 1;

	float delta = current[endIndex] - current[startIndex];
    if (direction * delta < 0 || Math.abs(delta) < thresh / 2)
        return 0;

    for (int i = startIndex; i < endIndex; i++) {
        if (direction >= 0) {
            // 递增条件break：第一个条件是防止小的抖动；第二个条件是防止细微递减
            if (current[i + 1] - current[i] < -thresh || current[i + 1] - current[startIndex] < -thresh)
                return 0;
        } else {
            // 递减条件break
            if (current[i + 1] - current[i] > thresh || current[i + 1] - current[startIndex] > thresh)
                return 0;
        }
    }
    return 1;
}

int WIDTH_SKIP = 0;

int arcuGetWidth(float *current, int currentLen, int arcFaultIndex) {

    if (arcFaultIndex >= currentLen)
        return -1;
    int breakCounter = 0, end = arcFaultIndex;
    float base = current[arcFaultIndex];
    int IEND = arcFaultIndex + currentLen;
    for (int i = arcFaultIndex + WIDTH_SKIP; i < IEND; i++) {
        if (base > 0) {
            if (current[i % currentLen] >= base) {
                breakCounter = 0;
                end = i;
            } else {
                breakCounter++;
            }
        } else {
            if (current[i % currentLen] <= base) {
                breakCounter = 0;
                end = i;
            } else {
                breakCounter++;
            }
        }
        if (breakCounter >= SEARCH_TOLERANCE) {
            break;
        }
    }
    return (end - arcFaultIndex + 1);
}

int arcuGetHealth(float *delta, float *absDelta, int len, float thresh) {

    int flip = 0;
    for (int i = 1; i < len; i++) {
        if (delta[i - 1] * delta[i] < 0 && absDelta[i - 1] >= thresh && absDelta[i] >= thresh) {
            flip++;
        }
    }
    return (len - flip) * 100 / len;
}

int arcuExtremeInRange(float *current, int currentLen, int arcFaultIndex, int range, float averageDelta) {
    if (arcFaultIndex < 1)
        return 1;
    float faultDelta = (float) (current[arcFaultIndex] - current[arcFaultIndex - 1]);
    float faultDeltaAbs = faultDelta > 0 ? faultDelta : -faultDelta;
    int direction = faultDelta > 0 ? 1 : -1;
    // log(current[arcFaultIndex - 1] + " " + current[arcFaultIndex] + " "
    // + current[arcFaultIndex + 1]);
    int i = arcFaultIndex + 1;
    float extreme = current[arcFaultIndex];
    int extIndex = -1;
    int mismatchCounter = 0;
    int checkedCounter = 0;
    // int shakeCounter = 0;

    int EXT_RANGE = range + SEARCH_TOLERANCE;
    float last1 = 0, last2 = 0;
    float shakeThresh = averageDelta;

    while (checkedCounter++ < EXT_RANGE) {

        if (i >= currentLen)
            i = 0;
        if ((current[i] - last1) * (last1 - last2) < 0
                && (arcuAbs(current[i] - last1) >= shakeThresh || arcuAbs(last1 - last2) >= shakeThresh)) {
            // shakeCounter++;
        }
        // if (shakeCounter >= 2)
        // break;

        if ((direction > 0 && current[i] >= extreme) || (direction < 0 && current[i] <= extreme)) { // 上升，寻找极大值;下降，寻找极小值
            extreme = current[i];
            extIndex = i;
            mismatchCounter = 0;
        } else {
            mismatchCounter++;
            // 反向跳动不可以太长
            if (mismatchCounter >= SEARCH_TOLERANCE)
                break;
            // 反向跳动不可以太大
            if ((direction > 0 && current[i] + faultDeltaAbs / 2 < extreme)
                    || (direction < 0 && current[i] - faultDeltaAbs / 2 > extreme)) {
                break;
            }
        }
        last2 = last1;
        last1 = current[i];
        i++;
    }
//    logd("extreme=" + extreme + " checkedCounter=" + checkedCounter + " i=" + i + " extIndex="
//            + extIndex);

    return extIndex;
}

// 极值点在查找范围内（ret=1），则认为是吸尘器等造成的电弧误报
int arcuIsMaxInRange(float *current, int currentLen, int arcFaultIndex, int range) {
//    logd("arcFaultIndex=" + arcFaultIndex + " range=" + range);
    if (arcFaultIndex < 1)
        return 1;
    int direction = current[arcFaultIndex] - current[arcFaultIndex - 1] > 0 ? 1 : -1;
    int i = arcFaultIndex + 1;
    float extreme = current[arcFaultIndex];
    int extIndex = -1;
    int mismatchCounter = 0;
    int checkedCounter = 0;

    while (checkedCounter++ < range) {

        if (i >= currentLen)
            i = 0;

        if ((direction > 0 && current[i] >= extreme) || (direction < 0 && current[i] <= extreme)) { // 上升，寻找极大值;下降，寻找极小值
            extreme = current[i];
            extIndex = i;
            mismatchCounter = 0;
        } else {
            mismatchCounter++;
            if (mismatchCounter >= 2)
                break;
        }
        i++;
    }
//    logd("extreme=" + extreme + " checkedCounter=" + checkedCounter + " i=" + i + " extIndex="
//            + extIndex);
    if (i - 1 == extIndex)
        return 0;
    return 1;
}

int arcuGetBigNum(float *in, int len, float thresh, float ratio) {

    int counter = 0;
    thresh *= ratio;
    for (int i = 0; i < len; i++) {
        if (in[i] >= thresh)
            counter++;
    }
    return counter;
}

/**
 *
 * @param inputs
 * @param len
 * @param absThresh
 *            绝对值的波动，比如设为20，则20以内的波动无论比值波动多少都不计
 * @param relaRatio
 *            相对比值波动，至少波动比例，百分比
 * @return
 */
char arcuIsStable(float *inputs, int len, float absThresh, int relativeRatio) {

    float average = 0, absAverage = 0;
    for (int i = 0; i < len; i++) {
        average += inputs[i];
    }
    average /= len;
    absAverage = average >= 0 ? average : -average;
    for (int i = 0; i < len; i++) {
        float absDelta = inputs[i] - average;
        absDelta = absDelta > 0 ? absDelta : -absDelta;

        if (absDelta > absThresh && absDelta * 100 > absAverage * relativeRatio) {
            return 0;
        }
    }
    return 1;
}

/**
 *
 * @param inputs
 * @param inputLen
 *            输入的长度
 * @param start
 *            计算的起点
 * @param len
 *            要计算的长度
 * @param ignoreThresh
 *            忽略多大的小波动
 * @return 最大波动的百分比
 */
float arcGetFluctuation(float* inputs, int inputLen, int start, int len, float ignoreThresh) {
    if (inputLen < len)
    return 0;
    float average = 0, absAverage = 0;
    for (int i = start; i < start + len; i++) {
        average += inputs[i % inputLen];
    }
    average /= len;
    absAverage = average >= 0 ? average : -average;
    float maxFluctuation = 0, tmp = 0;
    for (int i = start; i < start + len; i++) {
        float absDelta = inputs[i % inputLen] - average;
        absDelta = absDelta > 0 ? absDelta : -absDelta;
        tmp = absDelta / absAverage;
        if (absDelta >= ignoreThresh && tmp > maxFluctuation) {
            maxFluctuation = tmp;
        }
    }
    return maxFluctuation * 100;
}

/***
 * 为电弧记录量身定制
 *
 * @param inputs
 * @param inputLen
 * @param end
 *            [start,end)不包含当前的值
 * @param len
 * @param ignoreThresh
 * @return
 */
float arcLastestFluctuation(float *inputs, int inputLen, int end, int len, float ignoreThresh) {

    if (inputLen < len)
        return 0;
    int start = end - len >= 0 ? end - len : end + inputLen - len;

    float average = 0, absAverage = 0;
    for (int i = start; i < start + len; i++) {
        average += inputs[i % inputLen];
    }
    average /= len;
    absAverage = average >= 0 ? average : -average;
    float maxFluctuation = 0, tmp = 0;
    for (int i = start; i < start + len; i++) {
        float absDelta = inputs[i % inputLen] - average;
        absDelta = absDelta > 0 ? absDelta : -absDelta;
        tmp = absDelta / absAverage;
        if (absDelta >= ignoreThresh && tmp > maxFluctuation) {
            maxFluctuation = tmp;
        }
    }
    return maxFluctuation * 100;
}

float arcuAbs(float val) {
    return val >= 0 ? val : -val;
}

float arcuSqrt(float val) {
#ifdef ARM_MATH_CM4
    float sqrt = 0;
    arm_sqrt_f32(val, &sqrt);
    return sqrt;
#else
    return (float) sqrt(val);
#endif
}
