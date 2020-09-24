#include "arcfault_algo.h"
#include "arcfault_utils.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
//#define Malloc(type,n)  (type *)malloc((n)*sizeof(type))
#define CHECK_ITEM_NUM  10
/**
 * 可配区
 */
static const char VERSION[] = { 3, 0, 0, 0 };
static int gMinExtremeDis = 19; // 阻性电弧发生点到极值的最小距离
static int gMinWidth = 35; // 阻性电弧发生点最少宽度
static int gDelayCheckTime = 1000 / 20; // 延迟报警时间
static float gResJumpRatio = 3.5f; // 阻性负载最少跳变threshDelta的倍数
static int gAlarmThresh = 14; // 故障电弧报警阈值，大于10，默认14
static int gDutyRatioThresh = 92;
static int gArc2NumRatioThresh = 40;
static int gMaxSeriesThresh = 25;
static float gResFollowJumpMaxRatio = 3.5f; // 阻性负载跳变发生处,后续跳变的倍数不可大于跳变处，越大越难通过
static float gInductJumpRatio = 3.3f; // 感性负载最少跳变threshDelta的倍数
static float gResJumpThresh = 1.0f; // 阻性负载最小跳跃幅度，单位A
static float gInductJumpThresh = 2.5; // 阻性负载最小跳跃幅度，单位A
static float gInductMaxJumpRatio = 0.462; // 感性负载跳变值至少满足电流峰值的百分比，取50%低一点的值
static float gInductJumpMinThresh = 0.75f; // 感性负载待验证跳变值至少满足最大跳变值得百分比
static char gFftEnabled = 0;
static char gOverlayCheckEnabled = 0;
static float gParallelArcThresh = 48.0f;
static char gCheckEnabled[CHECK_ITEM_NUM];

/**
 * 自用区
 */
int *pBigJumpCounter = NULL; // 大跳跃的计数器
int *pAlarmCounter = NULL; // 报警计数器

static int gChannelNum = 1; // 通道数
static int *gWatingTime = NULL;
static int *gTimer = NULL;
const int ONE_EIGHT_PERIOD = 16; // 8分之1周期
const char LOG_ON = 1;

#define STATUS_NORMAL 0
#define STATUS_WAITING_CHECK 1
#define STATUS_IMMUNE 2
static int *gStatus = NULL;
static int *gArcNumAlarming = NULL; // 报警发生时的电弧数目
#define CYCLE_NUM_1S 50
static char **gResArcBuff = NULL;
static char **gInductArcBuff = NULL;
static int *gArcBuffIndex = NULL; // point to next write point
static int *gEffMonoIncCounter = NULL;
static int *gEffMonoDecCounter = NULL;
static char **gEasyArcBuff = NULL;

static float **gEffBuff = NULL;
static float **gAverageBuff = NULL;
static float **gHarmonicBuff = NULL;

static float **gAdditionalBuff = NULL;
static char *gIsFirst = NULL;
static char gOutMsg[64] = { 0 };

extern char *APP_BUILD_DATE;
char gIsLibExpired = 1;
const char *EXPIRED_DATE = "2021-12-30";
// prebuiltDate sample "Mar 03 2020"
int isExpired(const char *prebuiltDate, const char *expiredDate) {
    if (prebuiltDate == NULL || expiredDate == NULL)
        return 1;

    const static char MONTH_NAME[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
            "Oct", "Nov", "Dec" };
    char monthString[4] = { 0 };
    int prebuiltYear = 2020, prebuiltMonth = 4, prebuiltDay = 1;
    int expiredYear = 2020, expiredMonth = 1, expiredDay = 1;
    sscanf(prebuiltDate, "%s %d %d", monthString, &prebuiltDay, &prebuiltYear);

    for (int i = 0; i < 12; i++) {
        if (strncasecmp(monthString, MONTH_NAME[i], 3) == 0) {
            prebuiltMonth = i + 1;
            break;
        }
    }

    sscanf(expiredDate, "%d-%d-%d", &expiredYear, &expiredMonth, &expiredDay);
    int score = (expiredYear - prebuiltYear) * 31 * 12 + (expiredMonth - prebuiltMonth) * 31
            + (expiredDay - prebuiltDay);
    //得分小于0为过期
    return score < 0;
}

int arcAlgoStatus() {
    return gIsLibExpired;
}

//初始化
int arcAlgoInit(int channelNum) {
    //已经初始化过
    if (gTimer != NULL)
        return 1;
    //通道数不可以大于8
    if (channelNum >= 8)
        return -2;
    gIsLibExpired = isExpired(APP_BUILD_DATE, EXPIRED_DATE);

    gChannelNum = channelNum;
    pBigJumpCounter = (int*) malloc(sizeof(int) * gChannelNum); // 大跳跃的计数器
    memset(pBigJumpCounter, 0, sizeof(int) * gChannelNum);

    pAlarmCounter = (int*) malloc(sizeof(int) * gChannelNum); // 报警计数器
    memset(pAlarmCounter, 0, sizeof(int) * gChannelNum);

    gWatingTime = (int*) malloc(sizeof(int) * gChannelNum);
    memset(gWatingTime, 0, sizeof(int) * gChannelNum);

    gTimer = (int*) malloc(sizeof(int) * gChannelNum);
    memset(gTimer, 0, sizeof(int) * gChannelNum);

    gStatus = (int*) malloc(sizeof(int) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gStatus[i] = STATUS_NORMAL;
    }

    gArcNumAlarming = (int*) malloc(sizeof(int) * gChannelNum);
    memset(gArcNumAlarming, 0, sizeof(int) * gChannelNum);

    gResArcBuff = (char**) malloc(sizeof(char*) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gResArcBuff[i] = (char*) malloc(sizeof(char) * CYCLE_NUM_1S);
        memset(gResArcBuff[i], 0, sizeof(char) * CYCLE_NUM_1S);
    }

    gInductArcBuff = (char**) malloc(sizeof(char*) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gInductArcBuff[i] = (char*) malloc(sizeof(char) * CYCLE_NUM_1S);
        memset(gInductArcBuff[i], 0, sizeof(char) * CYCLE_NUM_1S);
    }

    gArcBuffIndex = (int*) malloc(sizeof(int) * gChannelNum);
    memset(gArcBuffIndex, 0, sizeof(int) * gChannelNum);

    gEffMonoIncCounter = (int*) malloc(sizeof(int) * gChannelNum);
    memset(gEffMonoIncCounter, 0, sizeof(int) * gChannelNum);

    gEffMonoDecCounter = (int*) malloc(sizeof(int) * gChannelNum);
    memset(gEffMonoDecCounter, 0, sizeof(int) * gChannelNum);

    gEasyArcBuff = (char**) malloc(sizeof(char*) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gEasyArcBuff[i] = (char*) malloc(sizeof(char) * CYCLE_NUM_1S);
        memset(gEasyArcBuff[i], 0, sizeof(char) * CYCLE_NUM_1S);
    }

    gEffBuff = (float**) malloc(sizeof(float*) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gEffBuff[i] = (float*) malloc(sizeof(float) * CYCLE_NUM_1S);
        memset(gEffBuff[i], 0, sizeof(float) * CYCLE_NUM_1S);
    }

    gAverageBuff = (float**) malloc(sizeof(float*) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gAverageBuff[i] = (float*) malloc(sizeof(float) * CYCLE_NUM_1S);
        memset(gAverageBuff[i], 0, sizeof(float) * CYCLE_NUM_1S);
    }

    gHarmonicBuff = (float**) malloc(sizeof(float*) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gHarmonicBuff[i] = (float*) malloc(sizeof(float) * CYCLE_NUM_1S);
        memset(gHarmonicBuff[i], 0, sizeof(float) * CYCLE_NUM_1S);
    }

    gAdditionalBuff = (float**) malloc(sizeof(float*) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gAdditionalBuff[i] = (float*) malloc(sizeof(float) * 5);
        memset(gAdditionalBuff[i], 0, sizeof(float) * 5);
    }

    gIsFirst = (char*) malloc(sizeof(char) * gChannelNum);
    memset(gIsFirst, 1, sizeof(char) * gChannelNum);

    memset(gCheckEnabled, 1, sizeof(char) * CHECK_ITEM_NUM);

    //内存分配失败
    if (gIsFirst == NULL)
        return -1;

    return 0;
}

int arcAnalyze(int channel, float *current, int length, int *outArcNum, int *thisPeriodNum) {
    float effCurrent = arcuEffectiveValue(current, length);
    return arcAnalyzeInner(channel, current, length, effCurrent, NULL, outArcNum, thisPeriodNum);
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
 * @return 是否需要故障报警,0不需要,1需要,-1未初始化
 */
int arcAnalyzeInner(int channel, float *current, const int length, float effCurrent, float *oddFft,
        int *outArcNum, int *thisPeriodNum) {
    if (gTimer == NULL || channel >= gChannelNum) {
        return -1;
    }

    // global timer
    gTimer[channel]++;
    gOutMsg[0] = '\0';
    // to protect lib, add random alarm for every 2.6days and 4.1days
    if (gIsLibExpired && (gTimer[channel] % 11232000 == 11231999 || gTimer[channel] % 17712000 == 17711999)) {
        if (outArcNum != NULL)
            *outArcNum = 14;
        if (thisPeriodNum != NULL)
            *thisPeriodNum = 1;
        return 1;
    }

    int resArcNum = 0, easyArcNum = 0, inductArcNum = 0;
    float d1[128 + 4]; // 一重微分
    float d1abs[128 + 4];
    float processCurrent[128 + 4];
    float maxD1abs = 0;

    // 防止跳变刚好发生在两个周期的交界处
    if (!gIsFirst[channel]) {
        for (int i = 0, j = 0; i < 128 + 4; i++) {
            if (i < 4) {
                processCurrent[i] = gAdditionalBuff[channel][i + 1]; // 从1开始跳过index0
            } else {
                processCurrent[i] = current[j++];
            }
        }
        d1[0] = gAdditionalBuff[channel][1] - gAdditionalBuff[channel][0];
        for (int i = 1; i < 128 + 4; i++) {
            d1[i] = processCurrent[i] - processCurrent[i - 1];
            d1abs[i] = d1[i] > 0 ? d1[i] : -d1[i];
            if (d1abs[i] > maxD1abs) {
                maxD1abs = d1abs[i];
            }
        }

    } else {
        for (int i = 0, j = 0; i < 128 + 4; i++) {
            if (i < 4) {
                processCurrent[i] = current[0]; // 从1开始跳过index0
            } else {
                processCurrent[i] = current[j++];
            }
        }
        d1[0] = 0;
        for (int i = 1; i < 128 + 4; i++) {
            d1[i] = processCurrent[i] - processCurrent[i - 1];
            d1abs[i] = d1[i] > 0 ? d1[i] : -d1[i];
            if (d1abs[i] > maxD1abs) {
                maxD1abs = d1abs[i];
            }
        }
        gIsFirst[channel] = 0;
    }

    for (int i = 0; i < 5; i++) {
        gAdditionalBuff[channel][i] = current[123 + i];
    }
    float averageDelta = arcuMean(d1abs, 128);
    float threshDelta = arcuThreshAverage(d1abs, 128, averageDelta / 2); // 除去平肩部分
    float effValue = effCurrent >= 0 ? effCurrent : arcuEffectiveValue(processCurrent, 128);

    float averageValue = arcuMean(processCurrent, 128);
    // 电流递增递减趋势判断
    int lastArcBuffIndex = gArcBuffIndex[channel] >= 1 ? gArcBuffIndex[channel] - 1 : CYCLE_NUM_1S - 1;
    if (effValue < gEffBuff[channel][lastArcBuffIndex] * 0.95f) {
        gEffMonoDecCounter[channel]++;
        gEffMonoIncCounter[channel] = 0;
    } else if (effValue > gEffBuff[channel][lastArcBuffIndex] * 1.05f) {
        gEffMonoIncCounter[channel]++;
        gEffMonoDecCounter[channel] = 0;
    } else {
        gEffMonoIncCounter[channel] = 0;
        gEffMonoDecCounter[channel] = 0;
    }

    float health = arcuGetHealth(d1, d1abs, 128, averageDelta / 3);
    if (maxD1abs > threshDelta * gResJumpRatio && maxD1abs > gResJumpThresh) {
        pBigJumpCounter[channel]++;
    }

    // 凸点检测，容感性负载
    if (maxD1abs >= 1.0f) {
        int lastBigJumpIndex = -1;
        double lastBigJumpVal = 0;
        for (int i = 0; i < 128; i++) {
            if (d1abs[i] > averageDelta * 5
                    || (fabs(lastBigJumpVal) > averageDelta * 10 && d1abs[i] > averageDelta * 4)) {

                if (lastBigJumpIndex >= 0 && d1[i] * lastBigJumpVal < 0 && i - lastBigJumpIndex <= 12
                        && i - lastBigJumpIndex >= 4) {
                    inductArcNum++;

                    i += 32;
                    lastBigJumpIndex = -1;
                    lastBigJumpVal = 0;
                    continue;
                }
                // 防止连续多个跳跃,在满足幅度要求下，取最近一个作为last
                if (d1abs[i] > lastBigJumpVal / 2) {
                    lastBigJumpIndex = i;
                    lastBigJumpVal = d1[i];
                }
            }
        }
    }

    // 阻性负载
    int checkFailed = 0, easyCheckFailed = 0; // 两套标准，其中一套略宽松标准
    if (health >= 70.0f && effValue >= 1.5f && maxD1abs > 1.0f) {
        // 最后3个点的电弧信息缺少，容易误判，忽略
        for (int i = 1; i < 128; i++) {
            if (d1abs[i] > gResJumpRatio * threshDelta && d1abs[i] >= gResJumpThresh) {
                int checkFailed = 0, easyCheckFailed = 0; // 两套标准，其中一套略宽松标准
                int faultIndex = i, extraTraverseSkip = 8;

                // 正击穿时要求故障电弧点电压为正,取0.1为近似值
                if ((checkFailed == 0 || easyCheckFailed == 0) && gCheckEnabled[ARC_CON_PN]) {
                    if ((d1[i] > 0 && (processCurrent[i - 1] < -0.1f || processCurrent[i] < 0.1f))
                            || (d1[i] < 0 && (processCurrent[i - 1] > 0.1f || processCurrent[i] > -0.1f))) {
                        checkFailed = ARC_CON_PN;
                        easyCheckFailed = ARC_CON_PN;
                    }
                }

                // 順延方向一致性检测
                if ((checkFailed == 0 || easyCheckFailed == 0) && gCheckEnabled[ARC_CON_CONS]) {

                    // 顺序方向
                    if (arcuIsConsistent(processCurrent, 128 + 4, d1[i], averageDelta, i, 4) == 0) {
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

                    int minExtremeDis = gMinExtremeDis;
                    extIndex = arcuExtremeInRange(processCurrent, 128, faultIndex, minExtremeDis,
                            averageDelta);

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

                    int minWidth = gMinWidth;
                    if (arcuGetWidth(processCurrent, 128, faultIndex) >= minWidth) {
                        // pass
                    } else {
                        checkFailed = ARC_CON_WIDT;
                    }
                }

                // 离散跳跃check，后续加和
                if (checkFailed == 0 && gCheckEnabled[ARC_CON_POSSUM]) {
                    double sum = 0;
                    for (int j = i + 1; j < i + 5 && j < 128 + 4; j++) {
                        sum += d1abs[j];
                    }
                    if (sum / d1abs[i] > 2) {
                        checkFailed = ARC_CON_POSSUM;
                        extraTraverseSkip = 0;
                    }
                }

                // 大跳跃数限制
                if (checkFailed == 0 && gCheckEnabled[ARC_CON_BJ]) {
                    int biggerNum = arcuGetBigNum(d1abs, 128, d1abs[i], 0.8f);
                    if (biggerNum >= 3) {
                        checkFailed = ARC_CON_BJ;
                    }
                }

                // 离散跳跃check，此后的跳跃*Ratio不可以比当前跳跃还大
                if (checkFailed == 0 && gCheckEnabled[ARC_CON_POSJ]) {
                    for (int j = i + 1; j < i + 4 && j < 128; j++) {
                        if (d1abs[j] * gResFollowJumpMaxRatio >= d1abs[i]) {
                            checkFailed = ARC_CON_POSJ;
                        }
                    }
                }

                if (checkFailed == 0) {
                    resArcNum++;
                    extraTraverseSkip = 30;
                }
                if (easyCheckFailed == 0) {
                    easyArcNum++;
                }
                i += extraTraverseSkip;
            }
        }

        if (resArcNum > 0) {
        }
    } else {
    }

    // to protect lib, remove induct arc detection
    if (gIsLibExpired) {
        inductArcNum /= 2;
    }

    // 一些重要状态变量赋值
    float harmonic = 0;
    gEffBuff[channel][gArcBuffIndex[channel]] = effValue;
    gAverageBuff[channel][gArcBuffIndex[channel]] = averageValue;
    if (gFftEnabled && oddFft != NULL) {
        gHarmonicBuff[channel][gArcBuffIndex[channel]] = 100 * (oddFft[1] + oddFft[2] + oddFft[3] + oddFft[4])
                / (oddFft[0] + 0.001f);
        harmonic = gHarmonicBuff[channel][gArcBuffIndex[channel]];
    }

    gResArcBuff[channel][gArcBuffIndex[channel]] = (char) (resArcNum);
    gInductArcBuff[channel][gArcBuffIndex[channel]] = (char) (inductArcNum);
    gEasyArcBuff[channel][gArcBuffIndex[channel]] = (char) easyArcNum;
    gArcBuffIndex[channel]++;
    if (gArcBuffIndex[channel] >= CYCLE_NUM_1S) {
        gArcBuffIndex[channel] = 0;
    }

    // 综合判断是否需要故障电弧报警

    int resArcNum1S = 0, inductArcNum1S = 0, parallelArcNum1S = 0, unbalanceNum1S = 0, workPeriodNum1S = 0,
            easyArcNum1S = 0;
    int dutyCounter = 0, dutyRatio = 0, tmpSeries = 0, maxSeries = 0;
    int resArcStart = -1, resArcEnd = -1, inductArcStart = -1, inductArcEnd = -1, resArcLen = 0,
            inductArcLen = 0;
    for (int i = gArcBuffIndex[channel]; i < gArcBuffIndex[channel] + CYCLE_NUM_1S; i++) {
        int index = i % CYCLE_NUM_1S;
        resArcNum1S += gResArcBuff[channel][index];
        easyArcNum1S += gEasyArcBuff[channel][index];
        inductArcNum1S += gInductArcBuff[channel][index];
        if (gEffBuff[channel][index] >= gParallelArcThresh) {
            parallelArcNum1S++;
        }
        if (gResArcBuff[channel][index] > 0) {
            if (resArcStart < 0) {
                resArcStart = i;
            }
            resArcEnd = i;
        }
        if (gInductArcBuff[channel][index] > 0) {
            if (inductArcStart < 0) {
                inductArcStart = i;
            }
            inductArcEnd = i;
        }
        // 避免和其他类型重复计算
        if (gEffBuff[channel][index] < gParallelArcThresh && gResArcBuff[channel][index] == 0
                && gInductArcBuff[channel][index] == 0 && gAverageBuff[channel][index] >= 1) {
            unbalanceNum1S++;
        }
        if (gEffBuff[channel][index] >= 4) {
            workPeriodNum1S++;
        }

        // if (gResArcBuff[channel][index] > 0) {
        // dutyCounter++;
        //
        // if (start < 0) {
        // start = i;
        // }
        // end = i;
        // tmpSeries++;
        // if (tmpSeries > maxSeries)
        // maxSeries = tmpSeries;
        // } else {
        // tmpSeries = 0;
        // }
    }
    inductArcLen = inductArcEnd - inductArcStart + 1;
    resArcLen = resArcEnd - resArcStart + 1;
    if (resArcEnd >= resArcStart) {
        dutyRatio = (dutyCounter * 100) / resArcLen;
    }

    int arcNum1S = resArcNum1S + inductArcNum1S + parallelArcNum1S + unbalanceNum1S;
    if (outArcNum != NULL)
        *outArcNum = arcNum1S;
    // 记录当前周期128个点的电弧数
    if (thisPeriodNum != NULL)
        *thisPeriodNum = resArcNum + inductArcNum;

    // 检测到电弧14个->进入待确认状态->在待确认期间发现不符合条件直接进入免疫->切出稳态后再继续进入正常检测期
    int fluctCheckEnd = gArcBuffIndex[channel], fluctCheckLen = CYCLE_NUM_1S; // totalLen > 3 ? totalLen - 3

    char parallelArcTrigger = 0;
    // 并联电弧
    if ((parallelArcNum1S >= 8 && (resArcNum1S + inductArcNum1S) >= 0 && unbalanceNum1S >= 2)
            || (parallelArcNum1S >= 7 && (resArcNum1S + inductArcNum1S) >= 1 && unbalanceNum1S >= 2)) {
        parallelArcTrigger = 1;
    }
    switch (gStatus[channel]) {
    case STATUS_NORMAL:
        // 进入免疫状态：1.占空比过大 2.一个周期检测到双弧超过40% 3.最大连续序列过长
        if (arcNum1S >= gAlarmThresh || parallelArcTrigger) {
            // 全是阻性负载电弧的话，要求基本稳定
            // if (inductArcNum1S == 0 && Utils.arcLastestFluctuation(gEffBuff[channel],
            // CYCLE_NUM_1S, fluctCheckEnd, fluctCheckLen, 0.2f) >= 8) {
            // gStatus[channel] = STATUS_IMMUNE;
            // break;
            // }

            gWatingTime[channel] = gTimer[channel];
            gStatus[channel] = STATUS_WAITING_CHECK;
            gArcNumAlarming[channel] = arcNum1S; // 记录当前电弧数目，留做报警时传递
        } else if (inductArcNum1S >= 3 && inductArcEnd - inductArcStart >= 6) {

            double averageBuffFluct = arcLastestFluctuation(gAverageBuff[channel],
            CYCLE_NUM_1S, (inductArcEnd + 1) % CYCLE_NUM_1S, inductArcLen, 0.2f);
            if (averageBuffFluct < 10)
                break;
            gWatingTime[channel] = gTimer[channel];
            gStatus[channel] = STATUS_WAITING_CHECK;
            gArcNumAlarming[channel] = 14; // 记录当前电弧数目，留做报警时传递
        }
        break;
    case STATUS_WAITING_CHECK:
        // to protect lib, remove fault alarm check
        if (gIsLibExpired) {
            return 1;
        }
        if (gTimer[channel] - gWatingTime[channel] > gDelayCheckTime) {
            gStatus[channel] = STATUS_NORMAL;
            pAlarmCounter[channel]++;
            if (outArcNum != NULL)
                *outArcNum = gArcNumAlarming[channel]; // 报警时使用确认点时记录的数目
            return 1;

        } else if (((inductArcNum1S + parallelArcNum1S) == 1000 && effValue > 1
                && (gEffMonoDecCounter[channel] == 0 || gEffMonoDecCounter[channel] >= 3)
                && arcLastestFluctuation(gEffBuff[channel], CYCLE_NUM_1S, fluctCheckEnd, fluctCheckLen,
                        0.2f) >= 8) || easyArcNum1S >= 25 || parallelArcNum1S >= 15) {
            gStatus[channel] = STATUS_IMMUNE;
            break;
        }
        break;
    case STATUS_IMMUNE:
        // 电流波动率超过5%或者谐波波动率超过10%
        if (arcLastestFluctuation(gEffBuff[channel], CYCLE_NUM_1S, fluctCheckEnd, fluctCheckLen, 0.2f) > 15
                && effCurrent >= 2) {
            gStatus[channel] = STATUS_NORMAL;
        }
        break;
    }
    // 并联电弧
    if ((parallelArcNum1S >= 7 && (resArcNum1S + inductArcNum1S) >= 1 && unbalanceNum1S >= 1)
            || (parallelArcNum1S >= 6 && (resArcNum1S + inductArcNum1S) >= 2 && unbalanceNum1S >= 2)) {
        return 1;
    }
    if (checkFailed > 0 || inductArcNum1S > 0) {
        snprintf(gOutMsg, 64, "ef=%.1f cf=%d ra=%d ia=%d\0", effValue, checkFailed, resArcNum, inductArcNum);
    }
    return 0;
}

/**
 * if no msg, outMsg[0] will be set to '\0';
 */
int arcAnalyzeWithMsg(int channel, float *current, const int length, float effCurrent, float *oddFft,
        int *outArcNum, int *thisPeriodNum, char *outMsg, int msgLen) {
    int ret = arcAnalyzeInner(channel, current, length, effCurrent, oddFft, outArcNum, thisPeriodNum);
    if (outMsg != NULL) {
        if (strlen(gOutMsg) > 0) {
            strncpy(outMsg, gOutMsg, msgLen < sizeof(gOutMsg) ? msgLen : sizeof(gOutMsg));
        } else {
            outMsg[0] = '\0';
        }
    }
    return ret;
}

void setArcMinExtremeDis(int minExtremeDis) {
    gMinExtremeDis = minExtremeDis;
}

void setArcMinWidth(int minWidth) {
    gMinWidth = minWidth;
}

void setArcDelayCheckTime(int delayCheckTime) {
    gDelayCheckTime = delayCheckTime;
}

void setArcResJumpRatio(float resJumpRatio) {
    gResJumpRatio = resJumpRatio;
}

void setArcAlarmThresh(int alarmThresh) {
    gAlarmThresh = alarmThresh;
}

void setArcDutyRatioThresh(int dutyRatioThresh) {
    gDutyRatioThresh = dutyRatioThresh;
}

void setArcArc2InWaveRatioThresh(int arc2NumRatioThresh) {
    gArc2NumRatioThresh = arc2NumRatioThresh;
}

void setArcMaxSeriesThresh(int maxSeriesThresh) {
    gMaxSeriesThresh = maxSeriesThresh;
}

void setArcResFollowJumpMaxRatio(float resFollowJumpMaxRatio) {
    gResFollowJumpMaxRatio = resFollowJumpMaxRatio;
}

void setArcInductJumpRatio(float inductJumpRatio) {
    gInductJumpRatio = inductJumpRatio;
}

void setArcResJumpThresh(float resJumpThresh) {
    gResJumpThresh = resJumpThresh;
}

void setArcInductJumpThresh(float inductJumpThresh) {
    gInductJumpThresh = inductJumpThresh;
}

void setArcInductMaxJumpRatio(float inductMaxJumpRatio) {
    gInductMaxJumpRatio = inductMaxJumpRatio;
}

void setArcInductJumpMinThresh(float inductJumpMinThresh) {
    gInductJumpMinThresh = inductJumpMinThresh;
}

void setArcFftEnabled(char fftEnabled) {
    gFftEnabled = fftEnabled;
}

void setArcCheckDisabled(int item) {
    if (item >= CHECK_ITEM_NUM)
        return;
    gCheckEnabled[item] = 0;
}

void setParallelArcThresh(float thresh) {
    gParallelArcThresh = thresh;
}

float getParallelArcThresh() {
    return gParallelArcThresh;
}

void setArcOverlayCheckEnabled(char enable) {
    gOverlayCheckEnabled = enable;
}

int getArcAlgoVersion() {
    return VERSION[0] << 24 | VERSION[1] << 16 | VERSION[2] << 8 | VERSION[3];
}
