#include "arcfault_algo.h"
#include "arcfault_utils.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
//#define Malloc(type,n)  (type *)malloc((n)*sizeof(type))
/**
 * 可配区
 */
static const char VERSION[] = { 1, 0, 0, 0 };
static int gMinExtremeDis = 19; // 阻性电弧发生点到极值的最小距离
static int gMinWidth = 35; // 阻性电弧发生点最少宽度
static int gCallPeriod = 20; // 调用间隔周期
static int gDelayCheckTime = 1000 / 20; // 延迟报警时间
static float gResJumpRatio = 4.0f; // 阻性负载最少跳变threshDelta的倍数
static int gAlarmThresh = 14; // 故障电弧报警阈值，大于10，默认14
static int gDutyRatioThresh = 92;
static int gArc2NumRatioThresh = 40;
static int gMaxSeriesThresh = 25;
static float gResFollowJumpMaxRatio = 3.5f; // 阻性负载跳变发生处,后续跳变的倍数不可大于跳变处，越大越难通过
static float gInductJumpRatio = 3.3f; // 感性负载最少跳变threshDelta的倍数
static float gResJumpThresh = 0.9; // 阻性负载最小跳跃幅度，单位A
static float gInductJumpThresh = 2.5; // 阻性负载最小跳跃幅度，单位A
static float gInductMaxJumpRatio = 0.462; // 感性负载跳变值至少满足电流峰值的百分比，取50%低一点的值
static float gInductJumpMinThresh = 0.75f; // 感性负载待验证跳变值至少满足最大跳变值得百分比
static char gFftEnabled = 0;

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
static char **gArcBuff = NULL;
static int *gArcBuffIndex = NULL; // point to next write point
static char **gEasyArcBuff = NULL;

#define MOREINFO_BUFF_NUM 20
static float **gEffBuff = NULL;
static float **gHarmonicBuff = NULL;
static int *gMoreInfoIndex = NULL; // point to next write point

static float *mLastPeriodPiont = NULL;
static char *gIsFirst = NULL;
// 稳态判断
static char *gIsStable = NULL;
static char *gIsHarmonicStable = NULL;

//只可以配置一次
void arcALgoInit(int channelNum) {
    if (gTimer != NULL)
        return;
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

    gArcBuff = (char**) malloc(sizeof(char*) * gChannelNum);
    for (int i = 0; i < gChannelNum; i++) {
        gArcBuff[i] = (char*) malloc(sizeof(char) * CYCLE_NUM_1S);
        memset(gArcBuff[i], 0, sizeof(char) * CYCLE_NUM_1S);
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
    // 稳态判断
    gIsStable = (char*) malloc(sizeof(char) * gChannelNum);
    memset(gIsStable, 1, sizeof(char) * gChannelNum);

    gIsHarmonicStable = (char*) malloc(sizeof(char) * gChannelNum);
    memset(gIsHarmonicStable, 1, sizeof(char) * gChannelNum);
}

char arcAnalyze(int channel, float *current, int length, int *outArcNum, int *thisPeriodNum) {
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
char arcAnalyzeInner(int channel, float *current, const int length, float effCurrent, float *oddFft,
        int *outArcNum, int *thisPeriodNum) {
    if (gTimer == NULL || channel >= gChannelNum) {
        return -1;
    }

    // global timer
    gTimer[channel]++;

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
    float threshDelta = arcuThreshAverage(d1abs, length, averageDelta); // 除去平肩部分
    float effValue = effCurrent >= 0 ? effCurrent : arcuEffectiveValue(current, length);

    float health = arcuGetHealth(d1, d1abs, length, averageDelta / 3);
    if (maxD1abs > threshDelta * gInductJumpRatio && maxD1abs > 0.9) {
        pBigJumpCounter++;
    }
    if (health >= 76 && effValue >= 1.5) {
        // 最后3个点的电弧信息缺少，容易误判，忽略
        const int PARTIAL_MAX_INDEX = 3;
        for (int i = 1; i < length - PARTIAL_MAX_INDEX; i++) {
            if (d1abs[i] > gResJumpRatio * threshDelta)
                if (d1abs[i - 1] < threshDelta) { // 击穿前基本为平肩，不用跳跃过大

                    if (d1abs[i] < gResJumpThresh) {
                        i += ONE_EIGHT_PERIOD;
                        continue;
                    }
                    int checkFailed = 0, easyCheckFailed = 0; // 两套标准，其中一套略宽松标准
                    int faultIndex = i;

                    // 正击穿时要求故障电弧点电压为正,取0.1为近似值
                    if (checkFailed == 0 || easyCheckFailed == 0) {
                        if ((d1[i] > 0 && (current[i - 1] < -0.1 || current[i] < 0.1))
                                || (d1[i] < 0 && (current[i - 1] > 0.1 || current[i] > -0.1))) {
                            checkFailed = 1;
                            easyCheckFailed = 1;
                        }
                    }

                    // 順延方向一致性检测
                    if (checkFailed == 0 || easyCheckFailed == 0) {

                        // 顺序方向
                        if (arcuIsConsistent(current, 128, d1[i], 0, i, 4) == 0) {
                            checkFailed = 2;
                            easyCheckFailed = 2;
                        }
                    }

                    // 如果有前向，前向也需要一致
                    if (checkFailed == 0 || easyCheckFailed == 0) {
                        if (d1abs[i - 1] > averageDelta && d1[i - 1] * d1[i] < 0) {
                            checkFailed = 3;
                            easyCheckFailed = 3;
                        }
                    }

                    // 极值点检查
                    // i是故障电弧发生点
                    int extIndex = faultIndex;
                    if (checkFailed == 0) {

                        extIndex = arcuExtremeInRange(current, length, faultIndex, gMinExtremeDis,
                                averageDelta);

                        if ((extIndex >= faultIndex + gMinExtremeDis)
                                || (extIndex < faultIndex && extIndex + length >= faultIndex + gMinExtremeDis)) {
                            // pass
                        } else {
                            // 距离极值点不足1/8周期，很可能是吸尘器开关电源等短尖周期的波形
                            checkFailed = 4;
                        }
                    }

                    // 宽度检查
                    // 有可能极值点通过，但波形从极值点迅速下降
                    if (checkFailed == 0) {

                        int width = arcuGetWidth(current, length, faultIndex);
                        if (width >= gMinWidth) {
                            // pass
                        } else {
                            checkFailed = 5;
                        }
                    }

                    // 大跳跃数限制
                    if (checkFailed == 0) {
                        int biggerNum = arcuGetBigNum(d1abs, length, d1abs[i], 0.8f);
                        if (biggerNum >= 3) {
                            checkFailed = 6;
                        }
                    }

                    // 离散跳跃check，此后的跳跃*Ratio不可以比当前跳跃还大
                    if (checkFailed == 0 || easyCheckFailed == 0) {
                        for (int j = i + 1; j < i + PARTIAL_MAX_INDEX && j < length; j++) {
                            if (d1abs[j] * gResFollowJumpMaxRatio >= d1abs[i]) {
                                checkFailed = 7;
                                easyCheckFailed = 7;
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
                                    && (i - lastBigJumpIndex <= 7 || length + lastBigJumpIndex - i <= 7)) {

                                inductArcNum++;
                            }

                        } else if (current[i] <= -averageDelta) {
                            // log(i + ": Neg half");
                            if (((lastBigJump > 0 && d1[i] < 0)
                                    || arcuIsConsistent(current, 128, d1[i], threshDelta, i, 5))
                                    && lastBigJumpIndex >= 0
                                    && (i - lastBigJumpIndex <= 7 || length + lastBigJumpIndex - i <= 7)) {
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
    gEffBuff[channel][gMoreInfoIndex[channel]] = effValue;
    if (gFftEnabled && oddFft != NULL) {
        gHarmonicBuff[channel][gMoreInfoIndex[channel]] = 100
                * (oddFft[1] + oddFft[2] + oddFft[3] + oddFft[4]) / (oddFft[0] + 0.001);
    }
    gMoreInfoIndex[channel]++;
    if (gMoreInfoIndex[channel] >= MOREINFO_BUFF_NUM)
        gMoreInfoIndex[channel] = 0;
    gIsStable[channel] = arcuIsStable(gEffBuff[channel], MOREINFO_BUFF_NUM, 0.2, 5);               // 稳态电流5%波动
    if (gFftEnabled && oddFft != NULL) {
        gIsHarmonicStable[channel] = arcuIsStable(gHarmonicBuff[channel],
        MOREINFO_BUFF_NUM, 2, 18);                        // 谐波率18%波动
    }

    gArcBuff[channel][gArcBuffIndex[channel]] = (char) (resArcNum + inductArcNum);
    gEasyArcBuff[channel][gArcBuffIndex[channel]] = (char) easyArcNum;
    gArcBuffIndex[channel]++;
    if (gArcBuffIndex[channel] >= CYCLE_NUM_1S) {
        gArcBuffIndex[channel] = 0;
    }

    // 综合判断是否需要故障电弧报警
    if (1) {
        int num = 0;
        int dutyCounter = 0;
        int tmpSeries = 0, maxSeries = 0;
        int start = -1, end = -1, dutyRatio = 0, have2Number = 0, totalLen = 0;
        for (int i = gArcBuffIndex[channel]; i < gArcBuffIndex[channel] + CYCLE_NUM_1S; i++) {
            int index = i % CYCLE_NUM_1S;
            num += gArcBuff[channel][index];
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
        // 记录当前1s内的总电弧数
        if (outArcNum != NULL)
            *outArcNum = num;
        // 记录当前周期128个点的电弧数
        if (thisPeriodNum != NULL)
            *thisPeriodNum = resArcNum + inductArcNum;

        // 检测到电弧14个->进入待确认状态->在待确认期间发现不符合条件直接进入免疫->切出稳态后再继续进入正常检测期
        switch (gStatus[channel]) {
        case STATUS_NORMAL:
            // 进入免疫状态：1.占空比过大 2.一个周期检测到双弧超过40% 3.最大连续序列过长
            if (num >= gAlarmThresh) {
                // 占空比不可以太高；1个全波内同时检测出2个电弧的比例不能太高；最大连续序列不能太大；高次谐波比不能是稳定态
                if (dutyRatio >= gDutyRatioThresh || have2Number * 100 / totalLen >= gArc2NumRatioThresh
                        || maxSeries >= gMaxSeriesThresh || (gFftEnabled && gIsHarmonicStable[channel])) {
                    gStatus[channel] = STATUS_IMMUNE;
                    break;
                }
                gWatingTime[channel] = gTimer[channel];
                gStatus[channel] = STATUS_WAITING_CHECK;
                gArcNumAlarming[channel] = num;                        // 记录当前电弧数目，留做报警时传递
            }
            break;
        case STATUS_WAITING_CHECK:
            if (gTimer[channel] - gWatingTime[channel] > gDelayCheckTime && effValue > 1.5f) {
                gStatus[channel] = STATUS_NORMAL;
                pAlarmCounter[channel]++;
                if (outArcNum != NULL)
                    *outArcNum = gArcNumAlarming[channel]; // 报警时使用确认点时记录的数目
                return 1;
            } else if (dutyRatio >= gDutyRatioThresh || have2Number * 100 / totalLen >= gArc2NumRatioThresh
                    || maxSeries >= gMaxSeriesThresh) {
                gStatus[channel] = STATUS_IMMUNE;
                break;
            }
            break;
        case STATUS_IMMUNE:
            if (!gIsStable[channel]) {
                gStatus[channel] = STATUS_NORMAL;
            }
            break;
        }
        return 0;
    }
    return 0;
}

void setArcMinExtremeDis(int minExtremeDis) {
    gMinExtremeDis = minExtremeDis;
}

void setArcMinWidth(int minWidth) {
    gMinWidth = minWidth;
}

void setArcCallPeriod(int callPeriod) {
    gCallPeriod = callPeriod;
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

int getArcAlgoVersion() {
    return VERSION[0] << 24 | VERSION[1] << 16 | VERSION[2] << 8 | VERSION[3];
}
