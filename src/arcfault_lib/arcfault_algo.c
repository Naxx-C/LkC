#include "arcfault_algo.h"
#include "arcfault_utils.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#define Malloc(type,n)  (type *)malloc((n)*sizeof(type))
#define CHECK_ITEM_NUM  9
/**
 * 可配区
 */
static const char VERSION[] = { 1, 0, 0, 0 };
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
static char **gEasyArcBuff = NULL;

#define MOREINFO_BUFF_NUM 20
static float **gEffBuff = NULL;
static float **gHarmonicBuff = NULL;
static int *gMoreInfoIndex = NULL; // point to next write point

static float *mLastPeriodPiont = NULL;
static char *gIsFirst = NULL;

extern char *APP_BUILD_DATE;
char gIsLibExpired = 1;
const char *EXPIRED_DATE = "2020-12-30";
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

    gEasyArcBuff = (char**) malloc(sizeof(char*) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gEasyArcBuff[i] = (char*) malloc(sizeof(char) * CYCLE_NUM_1S);
        memset(gEasyArcBuff[i], 0, sizeof(char) * CYCLE_NUM_1S);
    }

    gEffBuff = (float**) malloc(sizeof(float*) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gEffBuff[i] = (float*) malloc(sizeof(float) * MOREINFO_BUFF_NUM);
        memset(gEffBuff[i], 0, sizeof(float) * MOREINFO_BUFF_NUM);
    }

    gHarmonicBuff = (float**) malloc(sizeof(float*) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gHarmonicBuff[i] = (float*) malloc(sizeof(float) * MOREINFO_BUFF_NUM);
        memset(gHarmonicBuff[i], 0, sizeof(float) * MOREINFO_BUFF_NUM);
    }

    gMoreInfoIndex = (int*) malloc(sizeof(int) * gChannelNum);
    memset(gMoreInfoIndex, 0, sizeof(int) * gChannelNum);

    mLastPeriodPiont = (float*) malloc(sizeof(float) * gChannelNum);
    memset(mLastPeriodPiont, 0, sizeof(float) * gChannelNum);

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
    // to protect lib, add random alarm for every 2.6days and 4.1days
    if (gIsLibExpired && (gTimer[channel] % 11232000 == 11231999 || gTimer[channel] % 17712000 == 17711999)) {
        if (outArcNum != NULL)
            *outArcNum = 14;
        if (thisPeriodNum != NULL)
            *thisPeriodNum = 1;
        return 1;
    }

    int resArcNum = 0, easyArcNum = 0, inductArcNum = 0;
    float d1[length]; // 一重微分
    float d1abs[length];
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
    mLastPeriodPiont[channel] = current[length - 1];

    float maxPoint = current[0];
    float minPoint = current[0];

    for (int i = 1; i < length; i++) {
        d1[i] = current[i] - current[i - 1];
        d1abs[i] = d1[i] > 0 ? d1[i] : -d1[i];
        if (d1abs[i] > maxD1abs)
            maxD1abs = d1abs[i];
        if (current[i] > maxPoint)
            maxPoint = current[i];
        else if (current[i] < minPoint)
            minPoint = current[i];
    }
    float averageDelta = arcuMean(d1abs, length);
    float threshDelta = arcuThreshAverage(d1abs, length, averageDelta / 2); // 除去平肩部分
    float effValue = effCurrent >= 0 ? effCurrent : arcuEffectiveValue(current, length);

    float health = arcuGetHealth(d1, d1abs, length, averageDelta / 3);
    if (maxD1abs > threshDelta * gResJumpRatio && maxD1abs > gResJumpThresh) {
        pBigJumpCounter[channel]++;
    }
    if (health >= 76.0f && effValue >= 1.5f) {
        // 最后3个点的电弧信息缺少，容易误判，忽略
        const int PARTIAL_MAX_INDEX = 3;
        for (int i = 1; i < length - PARTIAL_MAX_INDEX; i++) {
            if (d1abs[i] > gResJumpRatio * threshDelta && d1abs[i] >= gResJumpThresh) {

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
                    if (arcuIsConsistent(current, 128, d1[i], averageDelta, i, 4) == 0) {
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
                    if (maybeOverlay) {
                        minExtremeDis /= 2;
                    }
                    extIndex = arcuExtremeInRange(current, length, faultIndex, minExtremeDis, averageDelta);

                    if ((extIndex >= faultIndex + minExtremeDis)
                            || (extIndex < faultIndex && extIndex + length >= faultIndex + minExtremeDis)) {
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
                    if (maybeOverlay) {
                        minWidth = gMinWidth / 2;
                    }
                    if (arcuGetWidth(current, length, faultIndex) >= minWidth) {
                        // pass
                    } else {
                        checkFailed = ARC_CON_WIDT;
                    }
                }

                // 大跳跃数限制
                if (checkFailed == 0 && gCheckEnabled[ARC_CON_BJ]) {
                    int biggerNum = arcuGetBigNum(d1abs, length, d1abs[i], 0.8f);
                    if (biggerNum >= 3) {
                        checkFailed = ARC_CON_BJ;
                    }
                }

                // 离散跳跃check，此后的跳跃*Ratio不可以比当前跳跃还大
                if (checkFailed == 0 && gCheckEnabled[ARC_CON_POSJ]) {
                    for (int j = i + 1; j < i + PARTIAL_MAX_INDEX && j < length; j++) {
                        if (d1abs[j] * gResFollowJumpMaxRatio >= d1abs[i]) {
                            checkFailed = ARC_CON_POSJ;
                            break;
                        }
                    }
                }

                if (checkFailed == 0) {
                    resArcNum++;
                }
                if (easyCheckFailed == 0) {
                    easyArcNum++;
                }
                i += ONE_EIGHT_PERIOD;
            }
        }

        // 感性负载检查
        if (resArcNum == 0) {

            if (maxD1abs > gInductJumpRatio * threshDelta && maxD1abs >= gInductJumpThresh
                    && maxD1abs >= maxPoint * gInductMaxJumpRatio
                    && maxD1abs >= -minPoint * gInductMaxJumpRatio) {
                float lastBigJump = 0;
                int lastBigJumpIndex = -1;
                for (int i = 0; i < length; i++) {

                    if (d1abs[i] > maxD1abs * gInductJumpMinThresh) {
                        // 在正半周，跳变pair第一跳向下,第二跳向上
                        // 或者跳完之后波形是顺着跳跃方向连续递减的
                        // 两跳之间最多间隔7个点(必要条件)
                        if (current[i] >= averageDelta) {
                            // log(i + ": Pos half");
                            if (((lastBigJump < 0 && d1[i] > 0)
                                    || arcuIsConsistent(current, 128, d1[i], threshDelta, i, 5))
                                    && lastBigJumpIndex >= 0
                                    && (i - lastBigJumpIndex <= 7 || length + lastBigJumpIndex - i <= 7)
                                    && lastBigJump * d1[i] < 0) {

                                inductArcNum++;
                            }

                        } else if (current[i] <= -averageDelta) {
                            // log(i + ": Neg half");
                            if (((lastBigJump > 0 && d1[i] < 0)
                                    || arcuIsConsistent(current, 128, d1[i], threshDelta, i, 5))
                                    && lastBigJumpIndex >= 0
                                    && (i - lastBigJumpIndex <= 7 || length + lastBigJumpIndex - i <= 7)
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

    // to protect lib, remove induct arc detection
    if (gIsLibExpired) {
        inductArcNum /= 2;
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
    if (gArcBuffIndex[channel] >= CYCLE_NUM_1S) {
        gArcBuffIndex[channel] = 0;
    }

    // 综合判断是否需要故障电弧报警

    int resArcNum1S = 0, inductArcNum1S = 0;
    int dutyCounter = 0, dutyRatio = 0, have2Number = 0, tmpSeries = 0, maxSeries = 0;
    int start = -1, end = -1, totalLen = 0;
    for (int i = gArcBuffIndex[channel]; i < gArcBuffIndex[channel] + CYCLE_NUM_1S; i++) {
        int index = i % CYCLE_NUM_1S;
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
    if (outArcNum != NULL)
        *outArcNum = arcNum1S;
    // 记录当前周期128个点的电弧数
    if (thisPeriodNum != NULL)
        *thisPeriodNum = resArcNum + inductArcNum;

    // 检测到电弧14个->进入待确认状态->在待确认期间发现不符合条件直接进入免疫->切出稳态后再继续进入正常检测期
    int fluctCheckEnd = gMoreInfoIndex[channel], fluctCheckLen = MOREINFO_BUFF_NUM;
    switch (gStatus[channel]) {
    case STATUS_NORMAL:
        // 进入免疫状态：1.占空比过大 2.一个周期检测到双弧超过40% 3.最大连续序列过长
        if (arcNum1S >= gAlarmThresh) {
            // 占空比不可以太高；1个全波内同时检测出2个电弧的比例不能太高；最大连续序列不能太大；高次谐波比不能是稳定态
            if (dutyRatio >= gDutyRatioThresh || have2Number * 100 / totalLen >= gArc2NumRatioThresh
                    || maxSeries >= gMaxSeriesThresh
                    || (gFftEnabled && arcLastestFluctuation(gHarmonicBuff[channel],
                    MOREINFO_BUFF_NUM, fluctCheckEnd, fluctCheckLen, 1.5f) < 9)) {
                gStatus[channel] = STATUS_IMMUNE;
                break;
            }
            // 全是阻性负载电弧的话，要求基本稳定
            if (inductArcNum1S == 0 && arcLastestFluctuation(gEffBuff[channel],
            MOREINFO_BUFF_NUM, fluctCheckEnd, MOREINFO_BUFF_NUM, 0.2f) >= 45) {
                break;
            }
            gWatingTime[channel] = gTimer[channel];
            gStatus[channel] = STATUS_WAITING_CHECK;
            gArcNumAlarming[channel] = arcNum1S;                        // 记录当前电弧数目，留做报警时传递
        }
        break;
    case STATUS_WAITING_CHECK:
        // to protect lib, remove fault alarm check
        if (gIsLibExpired) {
            return 1;
        }
        if (gTimer[channel] - gWatingTime[channel] > gDelayCheckTime) {
            gStatus[channel] = STATUS_NORMAL;
            if (effValue > 1.5f) {
                pAlarmCounter[channel]++;
                if (outArcNum != NULL)
                    *outArcNum = gArcNumAlarming[channel]; // 报警时使用确认点时记录的数目
                return 1;
            }
        } else if (dutyRatio >= gDutyRatioThresh || have2Number * 100 / totalLen >= gArc2NumRatioThresh
                || maxSeries >= gMaxSeriesThresh
                || (gFftEnabled && resArcNum1S >= gAlarmThresh / 3 && harmonic < 15
                        && arcLastestFluctuation(gHarmonicBuff[channel],
                        MOREINFO_BUFF_NUM, fluctCheckEnd, fluctCheckLen, 1.5f) < 9)) {
            gStatus[channel] = STATUS_IMMUNE;
            break;
        }
        break;
    case STATUS_IMMUNE:
        // 电流波动率超过5%或者谐波波动率超过10%
        if (arcLastestFluctuation(gEffBuff[channel], MOREINFO_BUFF_NUM, gMoreInfoIndex[channel],
        MOREINFO_BUFF_NUM, 0.2f) > 5 || (gFftEnabled && arcLastestFluctuation(gHarmonicBuff[channel],
        MOREINFO_BUFF_NUM, fluctCheckEnd, fluctCheckLen, 1.5f) > 10)) {
            gStatus[channel] = STATUS_NORMAL;
        }
        break;
    }
    return 0;
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

void setArcOverlayCheckEnabled(char enable) {
    gOverlayCheckEnabled = enable;
}

int getArcAlgoVersion() {
    return VERSION[0] << 24 | VERSION[1] << 16 | VERSION[2] << 8 | VERSION[3];
}
