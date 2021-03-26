#include "func_arcfault.h"
#include "power_utils.h"
#include "math_utils.h"
#include "algo_set_build.h"
#include "data_structure_utils.h"
#include "arcfault_smartmode.h"
#include "time_utils.h"
#include "log_utils.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#define Malloc(type,n)  (type *)malloc((n)*sizeof(type))
#define CHECK_ITEM_NUM  9
/**
 * 可配区
 */
static int gMinExtremeDis[CHANNEL_NUM]; //19 阻性电弧发生点到极值的最小距离
static int gMinWidth[CHANNEL_NUM]; // 35 阻性电弧发生点最少宽度
static int gDelayCheckTime[CHANNEL_NUM]; // 50 延迟报警时间
static float gResJumpRatio[CHANNEL_NUM]; //3.5f 阻性负载最少跳变threshDelta的倍数
static int gAlarmThresh[CHANNEL_NUM]; // 14  故障电弧报警阈值，大于10，默认14
static int gDutyRatioThresh[CHANNEL_NUM]; // 92
static int gArc2NumRatioThresh[CHANNEL_NUM]; // 40
static int gMaxSeriesThresh[CHANNEL_NUM]; // 25
static float gResFollowJumpMaxRatio[CHANNEL_NUM]; // 3.5f 阻性负载跳变发生处,后续跳变的倍数不可大于跳变处，越大越难通过
static float gInductJumpRatio[CHANNEL_NUM]; // 3.3f 感性负载最少跳变threshDelta的倍数
static float gResJumpThresh[CHANNEL_NUM]; // 1.0f 阻性负载最小跳跃幅度，单位A
static float gInductJumpThresh[CHANNEL_NUM]; // 2.5  阻性负载最小跳跃幅度，单位A
static float gInductMaxJumpRatio[CHANNEL_NUM]; // 0.52  感性负载跳变值至少满足电流峰值的百分比
static float gInductJumpMinThresh[CHANNEL_NUM]; // 0.75f   感性负载待验证跳变值至少满足最大跳变值得百分比
static char gFftEnabled = 0;
static char gOverlayCheckEnabled = 0;
static float gMaxCurrent = 100.0f;
static float gMinCurrent = 1.5f;

static char gCheckEnabled[CHECK_ITEM_NUM];
static int gSensitivity[CHANNEL_NUM];

static int gSmartmodeLearningTime[CHANNEL_NUM] = { 0 };
static int gSmartmodeLearningTimeSet[CHANNEL_NUM] = { 0 };
static int gSmartMode[CHANNEL_NUM];
static int gSmartmodeNumTriggerPerSecond = 38; //每秒检测出的电弧数目
static int gSmartmodeNumTriggerDuration = 60; //持续时间

/**
 * 自用区
 */
#define STATUS_NORMAL 0
#define STATUS_WAITING_CHECK 1
#define STATUS_IMMUNE 2
int pBigJumpCounter[CHANNEL_NUM] = { 0 }; // 大跳跃的计数器
int pAlarmCounter[CHANNEL_NUM] = { 0 }; // 报警计数器

static int gWatingTime[CHANNEL_NUM] = { 0 };
static int gTimer[CHANNEL_NUM] = { 0 };
const int ONE_EIGHT_PERIOD = 16; // 8分之1周期

static int gStatus[CHANNEL_NUM] = { 0 };
static int gArcNumAlarming[CHANNEL_NUM] = { 0 }; // 报警发生时的电弧数目
static char gResArcBuff[CHANNEL_NUM][50] = { 0 };
static char gInductArcBuff[CHANNEL_NUM][50] = { 0 };
static int gArcBuffIndex[CHANNEL_NUM] = { 0 }; // point to next write point
static char gEasyArcBuff[CHANNEL_NUM][50] = { 0 };

#define MOREINFO_BUFF_NUM 20
static float gEffBuff[CHANNEL_NUM][50] = { 0 };
static float gHarmonicBuff[CHANNEL_NUM][50] = { 0 };
static int gMoreInfoIndex[CHANNEL_NUM] = { 0 }; // point to next write point

static float mLastPeriodPiont[CHANNEL_NUM] = { 0 };
static char gIsFirst[CHANNEL_NUM] = { 0 };

static int gIsInitialized = 0;
static int gArcLastAlarmTime[CHANNEL_NUM] = { 0 };
#define ARC_TIME_TRIGGER_BUFFSIZE 24
static char gSmartmodeTimeTrigger[CHANNEL_NUM][ARC_TIME_TRIGGER_BUFFSIZE] = { 0 }; //24h内有4次触发(1个小时内重复触发不计)
static int gTimeTriggerBuffIndex[CHANNEL_NUM] = { 0 };
static int gLastTimeTriggerSectionTime[CHANNEL_NUM] = { 0 };
static int gLastTimeTriggerAlarmAuditTime[CHANNEL_NUM] = { 0 };
static int gSmartmodeNumTrigger[CHANNEL_NUM] = { 0 }; //连续1分钟内有每秒38次触发
static char gNumTriggerMissedCounter[CHANNEL_NUM] = { 0 }; //干扰中断计数器

const static int MIN = 60;
const static int HOUR = 3600;
const static int DAY = 3600 * 24;

static const int MODE_MAX = 2;
void setArcfaultSensitivity(int channel, int sensitivity) {
    if (sensitivity >= 0 && sensitivity <= MODE_MAX) {
        gSensitivity[channel] = sensitivity;
    }
    switch (sensitivity) {
    case ARCFAULT_SENSITIVITY_HIGH:
        gDelayCheckTime[channel] = 0;
        gMinExtremeDis[channel] = 15; //越大越严
        gMinWidth[channel] = 30; // 阻性电弧发生点最少宽度
        gResJumpRatio[channel] = 2.8f;
        gAlarmThresh[channel] = 10;
        gDutyRatioThresh[channel] = 200;
        gArc2NumRatioThresh[channel] = 200;
        gMaxSeriesThresh[channel] = 200;
        gResFollowJumpMaxRatio[channel] = 3.2f; // 阻性负载跳变发生处,后续跳变的倍数不可大于跳变处，越大越难通过
        gInductJumpRatio[channel] = 3.0f; // 感性负载最少跳变threshDelta的倍数
        gResJumpThresh[channel] = 0.9f; // 阻性负载最小跳跃幅度，单位A
        gInductJumpThresh[channel] = 2.0f; // 感性负载最小跳跃幅度，单位A
        gInductMaxJumpRatio[channel] = 0.40; // 越大越严，感性负载跳变值至少满足电流峰值的百分比，取50%低一点的值
        gInductJumpMinThresh[channel] = 0.70f; // 越大越严，感性负载待验证跳变值至少满足最大跳变值得百分比
        break;
    case ARCFAULT_SENSITIVITY_MEDIUM:
        gDelayCheckTime[channel] = 1000 / 20;
        gMinExtremeDis[channel] = 19;
        gMinWidth[channel] = 35;
        gResJumpRatio[channel] = 3.5f;
        gAlarmThresh[channel] = 14;
        gDutyRatioThresh[channel] = 92;
        gArc2NumRatioThresh[channel] = 40;
        gMaxSeriesThresh[channel] = 25;
        gResFollowJumpMaxRatio[channel] = 3.5f;
        gInductJumpRatio[channel] = 3.3f;
        gResJumpThresh[channel] = 1.0f;
        gInductJumpThresh[channel] = 2.5;
        gInductMaxJumpRatio[channel] = 0.52;
        gInductJumpMinThresh[channel] = 0.75f;
        break;
    case ARCFAULT_SENSITIVITY_LOW:
        gDelayCheckTime[channel] = 1000 / 20;
        gMinExtremeDis[channel] = 20;
        gMinWidth[channel] = 36;
        gResJumpRatio[channel] = 4.0f;
        gAlarmThresh[channel] = 14;
        gDutyRatioThresh[channel] = 90;
        gArc2NumRatioThresh[channel] = 35;
        gMaxSeriesThresh[channel] = 20;
        gResFollowJumpMaxRatio[channel] = 3.8f;
        gInductJumpRatio[channel] = 3.8f;
        gResJumpThresh[channel] = 1.5f;
        gInductJumpThresh[channel] = 3.0f;
        gInductMaxJumpRatio[channel] = 0.7f;
        gInductJumpMinThresh[channel] = 0.8f;
        break;
    default:
        break;
    }
}

static int getSmartModeTimeTriggerTimes(int channel, int alarmAction, int unixTime) {
    if (unixTime - gLastTimeTriggerSectionTime[channel] >= HOUR) {
        if (gTimeTriggerBuffIndex[channel] >= ARC_TIME_TRIGGER_BUFFSIZE
                || gTimeTriggerBuffIndex[channel] < 0) {
            gTimeTriggerBuffIndex[channel] = 0;
        }
        if (alarmAction == 1) {
            if (unixTime - gLastTimeTriggerAlarmAuditTime[channel] >= HOUR) {
                gSmartmodeTimeTrigger[channel][gTimeTriggerBuffIndex[channel]] = 1;
                gLastTimeTriggerAlarmAuditTime[channel] = unixTime;
#ifdef TMP_DEBUG
#if OUTLOG_ON
                if (outprintf != NULL) {
                    outprintf("TimeTrigger1 time=%d\r\n", gLastTimeTriggerAlarmAuditTime[channel]);
                }
#endif
#endif
            }
        } else {
            gSmartmodeTimeTrigger[channel][gTimeTriggerBuffIndex[channel]] = 0;
        }

        gTimeTriggerBuffIndex[channel]++;
        gLastTimeTriggerSectionTime[channel] = unixTime; //上一次处理记录时间
    } else if (alarmAction == 1) {
        int lastIndex = gTimeTriggerBuffIndex[channel] - 1;
        if (lastIndex < 0) {
            lastIndex = ARC_TIME_TRIGGER_BUFFSIZE - 1;
        }
        if (gSmartmodeTimeTrigger[channel][lastIndex] == 0
                && unixTime - gLastTimeTriggerAlarmAuditTime[channel] >= HOUR) {
            gSmartmodeTimeTrigger[channel][lastIndex] = 1;
            gLastTimeTriggerAlarmAuditTime[channel] = unixTime;
#ifdef TMP_DEBUG
#if OUTLOG_ON
            if (outprintf != NULL) {
                outprintf("TimeTrigger2 time=%d\r\n", gLastTimeTriggerAlarmAuditTime[channel]);
            }
#endif
#endif
        }
    }

    int smartmodeTimeTrigger = 0;
    for (int i = 0; i < ARC_TIME_TRIGGER_BUFFSIZE; i++) {
        smartmodeTimeTrigger += gSmartmodeTimeTrigger[channel][i];
    }

    return smartmodeTimeTrigger;
}

static int getHealth(float *delta, float *absDelta, int len, float thresh) {

    int flip = 0;
    for (int i = 1; i < len; i++) {
        if (delta[i - 1] * delta[i] < 0 && absDelta[i - 1] >= thresh && absDelta[i] >= thresh) {
            flip++;
        }
    }
    return (len - flip) * 100 / len;
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
static int isConsistent(const float *const current, int length, float direction, float thresh, int startIndex,
        int checkNum) {

    if (startIndex + checkNum > length)
        return 0;
    int endIndex = startIndex + checkNum - 1;

    if (direction * (current[endIndex] - current[startIndex]) < 0)
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

// 极值点在查找范围内，则认为是吸尘器等造成的电弧误报
static int SEARCH_TOLERANCE = 6;
static int isExtremeInRange(float *current, int currentLen, int arcFaultIndex, int range, float averageDelta) {
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
                && (fabs(current[i] - last1) >= shakeThresh || fabs(last1 - last2) >= shakeThresh)) {
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

static int WIDTH_SKIP = 0;
static int getWidth(float *current, int currentLen, int arcFaultIndex) {

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

static int getBigNum(float *in, int len, float thresh, float ratio) {

    int counter = 0;
    thresh *= ratio;
    for (int i = 0; i < len; i++) {
        if (in[i] >= thresh)
            counter++;
    }
    return counter;
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
static float getLastestFluctuation(float *inputs, int inputLen, int end, int len, float ignoreThresh) {

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

int initFuncArcfault(void) {

    //已经初始化过
    if (gIsInitialized)
        return 0;
    else
        gIsInitialized = 1;

    memset(gSmartmodeTimeTrigger, 0, sizeof(gSmartmodeTimeTrigger));
    for (int i = 0; i < CHANNEL_NUM; i++) {
        gStatus[i] = STATUS_NORMAL;
        gIsFirst[i] = 1;
        gSensitivity[i] = ARCFAULT_SENSITIVITY_MEDIUM;
        gSmartMode[i] = ARCFAULT_SMARTMODE_ON;
        gSmartmodeLearningTime[i] = 0;
        gSmartmodeLearningTimeSet[i] = 3 * DAY; // 3days
        gTimeTriggerBuffIndex[i] = 0;
        gLastTimeTriggerSectionTime[i] = 0;
        gLastTimeTriggerAlarmAuditTime[i] = 0;
        gArcLastAlarmTime[i] = 0;

        //算法阈值参数
        gMinExtremeDis[i] = 19; // 阻性电弧发生点到极值的最小距离
        gMinWidth[i] = 35; // 阻性电弧发生点最少宽度
        gDelayCheckTime[i] = 1000 / 20; // 延迟报警时间
        gResJumpRatio[i] = 3.5f; // 阻性负载最少跳变threshDelta的倍数
        gAlarmThresh[i] = 14; // 故障电弧报警阈值，大于10，默认14
        gDutyRatioThresh[i] = 92;
        gArc2NumRatioThresh[i] = 40;
        gMaxSeriesThresh[i] = 25;
        gResFollowJumpMaxRatio[i] = 3.5f; // 阻性负载跳变发生处,后续跳变的倍数不可大于跳变处，越大越难通过
        gInductJumpRatio[i] = 3.3f; // 感性负载最少跳变threshDelta的倍数
        gResJumpThresh[i] = 1.0f; // 阻性负载最小跳跃幅度，单位A
        gInductJumpThresh[i] = 2.5; // 阻性负载最小跳跃幅度，单位A
        gInductMaxJumpRatio[i] = 0.52; // 感性负载跳变值至少满足电流峰值的百分比
        gInductJumpMinThresh[i] = 0.75f; // 感性负载待验证跳变值至少满足最大跳变值得百分比
    }
    for (int i = 0; i < CHECK_ITEM_NUM; i++) {
        gCheckEnabled[i] = 1;
    }

    //内存分配失败
    if (gIsFirst == NULL)
        return -1;

    return 0;
}

/**
 * @param current
 *            一个采样周期的电流数据。
 * @param length
 *            采样电流数据长度。当前默认为128
 * @param effCurrent
 *            电流有效值。为避免重复计算，有计算过则直接传入，未计算过则传入-1
 * @param oddFft
 *            1,3,5,7,9次奇次谐波数据。有计算过则直接传入，否则设为NULL
 * @param outArcNum
 *            output检测到的电弧数目(不一定是故障电弧，好弧也包含在内),可设置为NULL
 * @return 是否需要故障报警,0不需要,1需要,2报警待确认中,-1未初始化
 */
int arcfaultDetect(int channel, int unixTime, DateStruct *ds, float *current, float *voltage, float effValue,
        float *oddFft, int *outArcNum, int *outThisPeriodNum, char *msg) {
    if (gTimer == NULL || channel >= CHANNEL_NUM || gIsInitialized == 0) {
        return -1;
    }

    // global timer
    gTimer[channel]++;

    int resArcNum = 0, easyArcNum = 0, inductArcNum = 0;
    float d1[128]; // 一重微分
    float d1abs[128];
    float maxD1abs = 0;

    // 防止跳变刚好发生在两个周期的交界处
    if (!gIsFirst[channel]) {
        d1[0] = current[0] - mLastPeriodPiont[channel];
        d1abs[0] = d1[0] > 0 ? d1[0] : -d1[0];
        maxD1abs = d1abs[0];
    } else {
        d1[0] = 0;
        d1abs[0] = 0;
        gIsFirst[channel] = 0;
    }
    mLastPeriodPiont[channel] = current[128 - 1];

    float maxPoint = current[0];
    float minPoint = current[0];

    for (int i = 1; i < 128; i++) {
        d1[i] = current[i] - current[i - 1];
        d1abs[i] = d1[i] > 0 ? d1[i] : -d1[i];
        if (d1abs[i] > maxD1abs)
            maxD1abs = d1abs[i];
        if (current[i] > maxPoint)
            maxPoint = current[i];
        else if (current[i] < minPoint)
            minPoint = current[i];
    }
    float averageDelta = getAverageValue(d1abs, 128);
    float threshDelta = getThreshAverage(d1abs, 128, averageDelta / 2); // 除去平肩部分

    float health = getHealth(d1, d1abs, 128, averageDelta / 3);
    if (maxD1abs > threshDelta * gResJumpRatio[channel] && maxD1abs > gResJumpThresh[channel]) {
        pBigJumpCounter[channel]++;
    }
    if (health >= 76.0f && effValue >= gMinCurrent && effValue <= gMaxCurrent) {
        // 最后3个点的电弧信息缺少，容易误判，忽略
        const int PARTIAL_MAX_INDEX = 3;
        for (int i = 1; i < 128 - PARTIAL_MAX_INDEX; i++) {
            if (d1abs[i] > gResJumpRatio[channel] * threshDelta && d1abs[i] >= gResJumpThresh[channel]) {

                int checkFailed = 0, easyCheckFailed = 0; // 两套标准，其中一套略宽松标准
                int faultIndex = i;
                char maybeOverlay = 0;
                if (gOverlayCheckEnabled
                        && (current[i - 1] > maxPoint / 4 || current[i - 1] < minPoint / 4)) {
                    maybeOverlay = 1;
                }

                // 击穿前基本为平肩，不用跳跃过大
                if ((checkFailed == 0 || easyCheckFailed == 0) && gCheckEnabled[ARC_CON_PREJ]) {
                    if (d1abs[i - 1] >= threshDelta) {
                        checkFailed = ARC_CON_PREJ;
                        easyCheckFailed = ARC_CON_PREJ;
                    }
                }

                // 正击穿时要求故障电弧点电压为正,取0.1为近似值
                if ((checkFailed == 0 || easyCheckFailed == 0) && gCheckEnabled[ARC_CON_PN]) {
                    if ((d1[i] > 0 && (current[i - 1] < -0.1f || current[i] < 0.1f))
                            || (d1[i] < 0 && (current[i - 1] > 0.1f || current[i] > -0.1f))) {
                        checkFailed = ARC_CON_PN;
                        easyCheckFailed = ARC_CON_PN;
                    }
                }

                // 順延方向一致性检测
                if ((checkFailed == 0 || easyCheckFailed == 0) && gCheckEnabled[ARC_CON_CONS]) {

                    // 顺序方向
                    if (isConsistent(current, 128, d1[i], averageDelta, i, 4) == 0) {
                        checkFailed = ARC_CON_CONS;
                        easyCheckFailed = ARC_CON_CONS;
                    }
                }

                // 如果有前向，前向也需要一致
                if ((checkFailed == 0 || easyCheckFailed == 0) && gCheckEnabled[ARC_CON_PREC]) {
                    if (d1abs[i - 1] > averageDelta && d1[i - 1] * d1[i] < 0) {
                        checkFailed = ARC_CON_PREC;
                        easyCheckFailed = ARC_CON_PREC;
                    }
                }

                // 极值点检查
                // i是故障电弧发生点
                int extIndex = faultIndex;
                if (checkFailed == 0 && gCheckEnabled[ARC_CON_EXTR]) {

                    int minExtremeDis = gMinExtremeDis[channel];
                    if (maybeOverlay) {
                        minExtremeDis /= 2;
                    }
                    extIndex = isExtremeInRange(current, 128, faultIndex, minExtremeDis, averageDelta);

                    if ((extIndex >= faultIndex + minExtremeDis)
                            || (extIndex < faultIndex && extIndex + 128 >= faultIndex + minExtremeDis)) {
                        // pass
                    } else {
                        // 距离极值点不足1/8周期，很可能是吸尘器开关电源等短尖周期的波形
                        checkFailed = ARC_CON_EXTR;
                    }
                }

                // 宽度检查
                // 有可能极值点通过，但波形从极值点迅速下降
                if (checkFailed == 0 && gCheckEnabled[ARC_CON_WIDT]) {

                    int minWidth = gMinWidth[channel];
                    if (maybeOverlay) {
                        minWidth = gMinWidth[channel] / 2;
                    }
                    if (getWidth(current, 128, faultIndex) >= minWidth) {
                        // pass
                    } else {
                        checkFailed = ARC_CON_WIDT;
                    }
                }

                // 大跳跃数限制
                if (checkFailed == 0 && gCheckEnabled[ARC_CON_BJ]) {
                    int biggerNum = getBigNum(d1abs, 128, d1abs[i], 0.8f);
                    if (biggerNum >= 3) {
                        checkFailed = ARC_CON_BJ;
                    }
                }

                // 离散跳跃check，此后的跳跃*Ratio不可以比当前跳跃还大
                if (checkFailed == 0 && gCheckEnabled[ARC_CON_POSJ]) {
                    for (int j = i + 1; j < i + PARTIAL_MAX_INDEX && j < 128; j++) {
                        if (d1abs[j] * gResFollowJumpMaxRatio[channel] >= d1abs[i]) {
                            checkFailed = ARC_CON_POSJ;
                            break;
                        }
                    }
                }

                if (checkFailed == 0) {
                    resArcNum++;
                }
#if LOG_ON
                else
                    printf("failed=%d index=%d I=%.2f\n", checkFailed, gTimer[channel] * 128 + faultIndex,
                            current[faultIndex]);
#endif
                if (easyCheckFailed == 0) {
                    easyArcNum++;
                }
                i += ONE_EIGHT_PERIOD;
            }
        }

        // 感性负载检查
        if (resArcNum == 0) {

            if (maxD1abs > gInductJumpRatio[channel] * threshDelta && maxD1abs >= gInductJumpThresh[channel]
                    && maxD1abs >= maxPoint * gInductMaxJumpRatio[channel]
                    && maxD1abs >= -minPoint * gInductMaxJumpRatio[channel]) {
                float lastBigJump = 0;
                int lastBigJumpIndex = -1;
                for (int i = 0; i < 128; i++) {

                    if (d1abs[i] > maxD1abs * gInductJumpMinThresh[channel]) {
                        // 在正半周，跳变pair第一跳向下,第二跳向上
                        // 或者跳完之后波形是顺着跳跃方向连续递减的
                        // 两跳之间最多间隔7个点(必要条件)
                        if (current[i] >= averageDelta) {
                            // log(i + ": Pos half");
                            if (((lastBigJump < 0 && d1[i] > 0)
                                    || isConsistent(current, 128, d1[i], threshDelta, i, 5))
                                    && lastBigJumpIndex >= 0
                                    && (i - lastBigJumpIndex <= 7 || 128 + lastBigJumpIndex - i <= 7)
                                    && lastBigJump * d1[i] < 0) {

                                inductArcNum++;
                            }

                        } else if (current[i] <= -averageDelta) {
                            // log(i + ": Neg half");
                            if (((lastBigJump > 0 && d1[i] < 0)
                                    || isConsistent(current, 128, d1[i], threshDelta, i, 5))
                                    && lastBigJumpIndex >= 0
                                    && (i - lastBigJumpIndex <= 7 || 128 + lastBigJumpIndex - i <= 7)
                                    && lastBigJump * d1[i] < 0) {
                                inductArcNum++;
                            }
                        }

                        lastBigJump = d1[i];
                        lastBigJumpIndex = i;
                    }
                }
                inductArcNum = inductArcNum > 2 ? 2 : inductArcNum;
            }
        }
    } else {
    }

    // 一些重要状态变量赋值
    float harmonic = 0;
    gEffBuff[channel][gMoreInfoIndex[channel]] = effValue;
    if (gFftEnabled && oddFft != NULL) {
        gHarmonicBuff[channel][gMoreInfoIndex[channel]] = 100
                * (oddFft[1] + oddFft[2] + oddFft[3] + oddFft[4]) / (oddFft[0] + 0.001f);
        harmonic = gHarmonicBuff[channel][gMoreInfoIndex[channel]];
    }
    gMoreInfoIndex[channel]++;
    if (gMoreInfoIndex[channel] >= MOREINFO_BUFF_NUM)
        gMoreInfoIndex[channel] = 0;

    gResArcBuff[channel][gArcBuffIndex[channel]] = (char) (resArcNum);
    gInductArcBuff[channel][gArcBuffIndex[channel]] = (char) (inductArcNum);
    gEasyArcBuff[channel][gArcBuffIndex[channel]] = (char) easyArcNum;
    gArcBuffIndex[channel]++;
    if (gArcBuffIndex[channel] >= 50) {
        gArcBuffIndex[channel] = 0;
    }

    // 综合判断是否需要故障电弧报警

    int resArcNum1S = 0, inductArcNum1S = 0;
    int dutyCounter = 0, dutyRatio = 0, have2Number = 0, tmpSeries = 0, maxSeries = 0;
    int start = -1, end = -1, totalLen = 0;
    for (int i = gArcBuffIndex[channel]; i < gArcBuffIndex[channel] + 50; i++) {
        int index = i % 50;
        resArcNum1S += gResArcBuff[channel][index];
        inductArcNum1S += gInductArcBuff[channel][index];
        if (gEasyArcBuff[channel][index] > 0) {
            dutyCounter++;
            if (gEasyArcBuff[channel][index] > 1)
                have2Number++;
            if (start < 0) {
                start = i;
            }
            end = i;
            tmpSeries++;
            if (tmpSeries > maxSeries)
                maxSeries = tmpSeries;
        } else {
            tmpSeries = 0;
        }
    }
    totalLen = end - start + 1;
    if (end >= start) {
        dutyRatio = (dutyCounter * 100) / totalLen;
    }
    // 给感性电弧1.5倍的额外权重,保障即使9个及以下的感性电弧也不会报警
    int arcNum1S = resArcNum1S + inductArcNum1S * 3 / 2;
    int arcNumThisPeriod = resArcNum + inductArcNum;
    if (outArcNum != NULL)
        *outArcNum = arcNum1S;
    // 记录当前周期128个点的电弧数
    if (outThisPeriodNum != NULL)
        *outThisPeriodNum = arcNumThisPeriod;

    // 检测到电弧14个->进入待确认状态->在待确认期间发现不符合条件直接进入免疫->切出稳态后再继续进入正常检测期
    int fluctCheckEnd = gMoreInfoIndex[channel], fluctCheckLen = MOREINFO_BUFF_NUM;
    int alarmAction = ARCFAULT_ACTION_NONE;
    switch (gStatus[channel]) {
    case STATUS_NORMAL:
        // 进入免疫状态：1.占空比过大 2.一个周期检测到双弧超过40% 3.最大连续序列过长
        if (arcNum1S >= gAlarmThresh[channel]) {
            // 占空比不可以太高；1个全波内同时检测出2个电弧的比例不能太高；最大连续序列不能太大；高次谐波比不能是稳定态
            if (dutyRatio >= gDutyRatioThresh[channel]
                    || have2Number * 100 / totalLen >= gArc2NumRatioThresh[channel]
                    || maxSeries >= gMaxSeriesThresh[channel]
                    || (gFftEnabled && getLastestFluctuation(gHarmonicBuff[channel],
                    MOREINFO_BUFF_NUM, fluctCheckEnd, fluctCheckLen, 1.5f) < 9)) {
                gStatus[channel] = STATUS_IMMUNE;
#if TMP_DEBUG
#if OUTLOG_ON
                if (outprintf != NULL) {
                    outprintf("immune1 dr=%d h2n=%d ms=%d\r\n", dutyRatio, have2Number, maxSeries);
                }
#endif
#endif
                break;
            }
            // 全是阻性负载电弧的话，要求基本稳定
//            if (inductArcNum1S == 0 && getLastestFluctuation(gEffBuff[channel],
//            MOREINFO_BUFF_NUM, fluctCheckEnd, MOREINFO_BUFF_NUM, 0.2f) >= 45) {
//#if TMP_DEBUG
//#if OUTLOG_ON
//                if (outprintf != NULL) {
//                    outprintf("resarc but not s\r\n");
//                }
//#endif
//#endif
//                break;
//            }
            gWatingTime[channel] = gTimer[channel];
            gStatus[channel] = STATUS_WAITING_CHECK;
            gArcNumAlarming[channel] = arcNum1S; // 记录当前电弧数目，留做报警时传递
        }
        break;
    case STATUS_WAITING_CHECK:
        if (gTimer[channel] - gWatingTime[channel] > gDelayCheckTime[channel]) {
            gStatus[channel] = STATUS_NORMAL;
            if (effValue > 1.5f) {
                pAlarmCounter[channel]++;
                if (outArcNum != NULL)
                    *outArcNum = gArcNumAlarming[channel]; // 报警时使用确认点时记录的数目
                alarmAction = ARCFAULT_ACTION_ALARM;
                break;
            }
        } else if (dutyRatio >= gDutyRatioThresh[channel]
                || have2Number * 100 / totalLen >= gArc2NumRatioThresh[channel]
                || maxSeries >= gMaxSeriesThresh[channel]
                || (gFftEnabled && resArcNum1S >= gAlarmThresh[channel] / 3 && harmonic < 15
                        && getLastestFluctuation(gHarmonicBuff[channel],
                        MOREINFO_BUFF_NUM, fluctCheckEnd, fluctCheckLen, 1.5f) < 9)) {
            gStatus[channel] = STATUS_IMMUNE;
#if TMP_DEBUG
#if OUTLOG_ON
            if (outprintf != NULL) {
                outprintf("immune2 dr=%d h2n=%d ms=%d\r\n", dutyRatio, have2Number, maxSeries);
            }
#endif
#endif
            break;
        }
        break;
    case STATUS_IMMUNE:
        // 电流波动率超过5%或者谐波波动率超过10%
        if (getLastestFluctuation(gEffBuff[channel], MOREINFO_BUFF_NUM, gMoreInfoIndex[channel],
        MOREINFO_BUFF_NUM, 0.2f) > 5 || (gFftEnabled && getLastestFluctuation(gHarmonicBuff[channel],
        MOREINFO_BUFF_NUM, fluctCheckEnd, fluctCheckLen, 1.5f) > 10)) {
            gStatus[channel] = STATUS_NORMAL;
        }
        break;
    }

    //智能模式
    if (gSmartMode[channel] == ARCFAULT_SMARTMODE_ON) {

        if (arcNum1S >= 8 && arcNumThisPeriod > 0) {
            int zeroCross = getZeroCrossIndex(voltage, 0, 128);
            ArcCalFeature(channel, current, zeroCross, 128, effValue, arcNum1S, arcNumThisPeriod);
        }

        if (alarmAction == ARCFAULT_ACTION_ALARM) {
            int smartmodeAlarmAction = alarmAction;
            if (gSmartmodeLearningTime[channel] > 0) {
                smartmodeAlarmAction = ArcStudyAnalysis(channel, 1);
            } else {
                smartmodeAlarmAction = ArcStudyAnalysis(channel, 0);
            }
            alarmAction = smartmodeAlarmAction; //智能模式调整报警动作
        }

        //学习模式倒计时
        if (gSmartmodeLearningTime[channel] > 0) {
            if (gTimer[channel] % 50 == 49) {
                gSmartmodeLearningTime[channel]--;
            }
        }

        //24小时内触发4次以上
        int smartmodeTimeTrigger = getSmartModeTimeTriggerTimes(channel, alarmAction, unixTime);

        //连续超过阈值60*1次
        if (gTimer[channel] % 50 == 49) {
            if (arcNum1S >= gSmartmodeNumTriggerPerSecond) {
                gSmartmodeNumTrigger[channel]++;
                gNumTriggerMissedCounter[channel] = 0;
#if OUTLOG_ON
                if (outprintf != NULL) {
                    outprintf("nc=%d gt=%d\r\n", gSmartmodeNumTrigger[channel],gTimer[channel]);
                }
#endif
            } else {
                if (gNumTriggerMissedCounter[channel] < 5)
                    gNumTriggerMissedCounter[channel]++;

                if (gNumTriggerMissedCounter[channel] >= 2) //连续2s不满足，清0
                    gSmartmodeNumTrigger[channel] = 0;
            }
        }

        // 24小时内有4小时触发过报警,进入学习模式
        static int lastSmartmodeTimeTrigger[CHANNEL_NUM] = { 0 };
        if (smartmodeTimeTrigger >= 4&& smartmodeTimeTrigger != lastSmartmodeTimeTrigger[channel]
        && gSmartmodeLearningTime[channel] <= 0 && alarmAction == ARCFAULT_ACTION_ALARM) {
            startArcLearning(channel);
        }
#if OUTLOG_ON
        if (outprintf != NULL) {
            if (smartmodeTimeTrigger >= 1 && smartmodeTimeTrigger != lastSmartmodeTimeTrigger[channel])
                outprintf("smartmodeTimeTrigger[%d]=%d last=%d\r\n", channel, smartmodeTimeTrigger,
                        lastSmartmodeTimeTrigger[channel]);
        }
#endif
        lastSmartmodeTimeTrigger[channel] = smartmodeTimeTrigger;
        // 连续1分钟电弧数超过阈值,进入学习模式
        if (gSmartmodeNumTrigger[channel] >= gSmartmodeNumTriggerDuration
                && gSmartmodeLearningTime[channel] <= 0) {
            startArcLearningWithTime(channel, 3 * MIN);
        }
    }

    if (alarmAction == ARCFAULT_ACTION_ALARM) {
#if TMP_DEBUG
#if OUTLOG_ON
        if (unixTime - gArcLastAlarmTime[channel] > 0) {
            if (outprintf != NULL) {
                outprintf("alarmAction=%d\r\n", alarmAction);
            }
        }
#endif
#endif
        gArcLastAlarmTime[channel] = unixTime; //上一次报警时间

    }
    return alarmAction;
}

//智能模式使能. 如演示等非正常环境使用时,建议临时关闭
int setArcfaultSmartMode(int channel, int mode) {
    if (mode > 1 || mode < 0)
        return -1;
    gSmartMode[channel] = mode;
    return 0;
}

void setArcMinExtremeDis(int minExtremeDis) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gMinExtremeDis[i] = minExtremeDis;
}

void setArcMinWidth(int minWidth) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gMinWidth[i] = minWidth;
}

void setArcDelayCheckTime(int delayCheckTime) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gDelayCheckTime[i] = delayCheckTime;
}

void setArcResJumpRatio(float resJumpRatio) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gResJumpRatio[i] = resJumpRatio;
}

void setArcAlarmThresh(int alarmThresh) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gAlarmThresh[i] = alarmThresh;
}

void setArcDutyRatioThresh(int dutyRatioThresh) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gDutyRatioThresh[i] = dutyRatioThresh;
}

void setArcArc2InWaveRatioThresh(int arc2NumRatioThresh) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gArc2NumRatioThresh[i] = arc2NumRatioThresh;
}

void setArcMaxSeriesThresh(int maxSeriesThresh) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gMaxSeriesThresh[i] = maxSeriesThresh;
}

void setArcResFollowJumpMaxRatio(float resFollowJumpMaxRatio) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gResFollowJumpMaxRatio[i] = resFollowJumpMaxRatio;
}

void setArcInductJumpRatio(float inductJumpRatio) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gInductJumpRatio[i] = inductJumpRatio;
}

void setArcResJumpThresh(float resJumpThresh) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gResJumpThresh[i] = resJumpThresh;
}

void setArcInductJumpThresh(float inductJumpThresh) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gInductJumpThresh[i] = inductJumpThresh;
}

void setArcInductMaxJumpRatio(float inductMaxJumpRatio) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gInductMaxJumpRatio[i] = inductMaxJumpRatio;
}

void setArcInductJumpMinThresh(float inductJumpMinThresh) {
    for (int i = 0; i < CHANNEL_NUM; i++)
        gInductJumpMinThresh[i] = inductJumpMinThresh;
}

void setArcFftEnabled(char fftEnabled) {
    gFftEnabled = fftEnabled;
}

void setArcCheckDisabled(int item) {
    if (item >= CHECK_ITEM_NUM)
        return;
    gCheckEnabled[item] = 0;
}

void setArcOverlayCheckEnabled(char enable) {
    gOverlayCheckEnabled = enable;
}

void setArcCurrentRange(float minCurrent, float maxCurrent) {

    gMinCurrent = minCurrent;
    gMaxCurrent = maxCurrent;
}

void setArcSmartmodeNumTriggerThresh(int numPerSecond, int duration) {
    gSmartmodeNumTriggerPerSecond = numPerSecond;
    gSmartmodeNumTriggerDuration = duration;
}

void setArcLearningTime(int channel, int learningTime) {
    gSmartmodeLearningTimeSet[channel] = learningTime;
}

int getArcLearningRemainingTime(int channel) {
    return gSmartmodeLearningTime[channel];
}

void startArcLearning(int channel) {
    if (gSmartMode[channel] == ARCFAULT_SMARTMODE_OFF)
        return;
#if OUTLOG_ON
    if (outprintf != NULL) {
        outprintf("start arclearning %d\r\n", gSmartmodeLearningTimeSet[channel]);
    }
#endif
    gSmartmodeLearningTime[channel] = gSmartmodeLearningTimeSet[channel];
}

void startArcLearningWithTime(int channel, int time) {
    if (gSmartMode[channel] == ARCFAULT_SMARTMODE_OFF)
        return;
#if OUTLOG_ON
    if (outprintf != NULL) {
        outprintf("start arclearning %d\r\n", time);
    }
#endif
    gSmartmodeLearningTime[channel] = time;
}

void stopArcLearning(int channel) {
    gSmartmodeLearningTime[channel] = 0;
}

void clearArcLearningResult(void) {
    ClearArcFeature();
}
