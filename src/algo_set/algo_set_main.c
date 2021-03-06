#include <data_structure_utils.h>
#include "file_utils.h"
#include "string_utils.h"
#include "time_utils.h"
#include "power_utils.h"
#include "log_utils.h"
#include "algo_set.h"
#include "algo_set_internal.h"
#include "algo_set_build.h"
#include "algo_base_struct.h"
#include "func_charging_alarm.h"
#include "func_dorm_converter.h"
#include "func_malicious_load.h"
#include "func_arcfault.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef ARM_MATH_CM4
#include "arm_math.h"
#else
#include <math.h>
#include "fft.h"
#endif

/**固定定义区*/
const static char VERSION[] = { 1, 0, 0, 7 };
static const int B_MAX[3] = { 2021, 12, 30 }; //最大允许集成编译时间,其他地方是障眼法

/**运行变量区*/
#define WAVE_BUFF_NUM 512
#define EFF_BUFF_NUM (50 * 1) // effective current buffer NUM
static float gActivePBuff[CHANNEL_NUM][EFF_BUFF_NUM];
static float gIWaveBuff[CHANNEL_NUM][WAVE_BUFF_NUM]; // FIFO buff, pos0是最老的数据
static float gUWaveBuff[CHANNEL_NUM][WAVE_BUFF_NUM];
static float gLastStableIWaveBuff[CHANNEL_NUM][WAVE_BUFF_NUM];
static float gLastStableUWaveBuff[CHANNEL_NUM][WAVE_BUFF_NUM];
static float gLastStableIEff[CHANNEL_NUM];
static float gLastStableUEff[CHANNEL_NUM];
static float gDeltaIWaveBuff[CHANNEL_NUM][WAVE_BUFF_NUM];
static float gLastStableCurSamplePosTop[CHANNEL_NUM]; //上个稳态的电流采集点的正的最大值
static float gLastStableCurSampleNegTop[CHANNEL_NUM]; //上个稳态的电流采集点的负的最大值

static int gLastStable[CHANNEL_NUM]; // 上个周期的稳态情况
static int gTransitTime[CHANNEL_NUM]; // 负荷变化过渡/非稳态时间
static float gTransitTmpIMax[CHANNEL_NUM]; //电流变化过渡期有效值的最大值
static float gTransitCurSamplePosTopMax[CHANNEL_NUM]; //电流变化过渡期采样点的正的最大值
static float gTransitCurSampleNegTopMax[CHANNEL_NUM]; //电流变化过渡期采样点的正的最大值

static float gLastStableActivePower[CHANNEL_NUM]; //稳态窗口下最近的有功功率,对于缓慢变化的场景会跟随变化
static float gLastProcessedStableActivePower[CHANNEL_NUM]; //上次经过稳态事件处理的有功功率,对于缓慢变化的场景不会跟随变化

static int gTimer[CHANNEL_NUM];
static char gIsLibExpired = 1;
//static int gDeltaFftCalVoter = 0; //投票决定是否要对差分波形进行fft运算

//算法配置和结果
static int gChargingAlarm[CHANNEL_NUM];
static int gFuncChargingAlarmEnabled = 0;

static int gDormConverterAlarm[CHANNEL_NUM];
static int gDormConverterLastAlarmTime[CHANNEL_NUM];
static int gFuncDormConverterEnabled = 0;

static int gMaliLoadAlarm[CHANNEL_NUM];
static int gFuncMaliciousLoadEnabled = 0;

static int gArcfaultAlarm[CHANNEL_NUM];
static int gFuncArcfaultEnabled = 0;
static int gArcNum[CHANNEL_NUM], gThisPeriodNum[CHANNEL_NUM];

static NilmCloudFeature gNilmCloudFeature[CHANNEL_NUM];
static int gNilmCloudFeatureEnabled = 0;

#define SHORT_TRACK_SIZE 10 // 1s采样一次，10s数据
static PowerTrack gShortPowerTrack[CHANNEL_NUM][SHORT_TRACK_SIZE]; //存储电能足迹
static int gShortTrackNum[CHANNEL_NUM];

#define POWER_TRACK_SIZE (60*6) // 10s采样一次，60min数据
static PowerTrack gPowerTrack[CHANNEL_NUM][POWER_TRACK_SIZE]; //存储电能足迹
static int gPowerTrackNum[CHANNEL_NUM];

static float gPowerCost[CHANNEL_NUM]; //时间内线路总功耗kws
static int gLastPowercostUpdateTime[CHANNEL_NUM]; //上一次更新功耗时的线路总的有功功率
static float gLastActivePower[CHANNEL_NUM];

/**可配变量区*/
static float gMinEventDeltaPower[CHANNEL_NUM];

int setModuleEnable(int module, int enable) {

    switch (module) {
    case ALGO_CHARGING_DETECT:
        gFuncChargingAlarmEnabled = enable;
        break;
    case ALGO_DORM_CONVERTER_DETECT:
        gFuncDormConverterEnabled = enable;
        break;
    case ALGO_MALICIOUS_LOAD_DETECT:
        gFuncMaliciousLoadEnabled = enable;
        break;
    case ALGO_ARCFAULT_DETECT:
        gFuncArcfaultEnabled = enable;
        break;
    case ALGO_NILM_CLOUD_FEATURE:
        gNilmCloudFeatureEnabled = enable;
        break;
    default:
        return -1;
    }
#if TMP_DEBUG
#if OUTLOG_ON
    if (outprintf != NULL) {
        outprintf("module=%d enable=%d\r\n", module, enable);
    }
#endif
#endif
    return 0;
}

int getChargingDetectResult(int channel) {
    return gChargingAlarm[channel];
}

int getDormConverterDetectResult(int channel) {
    return gDormConverterAlarm[channel];
}

int getMaliLoadDetectResult(int channel) {
    return gMaliLoadAlarm[channel];
}

int getArcfaultDetectResult(int channel, int *arcNum, int *onePeriodNum) {
    if (arcNum != NULL)
        *arcNum = gArcNum[channel];
    if (onePeriodNum != NULL)
        *onePeriodNum = gThisPeriodNum[channel];
    return gArcfaultAlarm[channel];
}

void getNilmCloudFeature(int channel, NilmCloudFeature **nilmCloudFeature) {
    if (!gNilmCloudFeatureEnabled || gNilmCloudFeature[channel].deltaActivePower < LF) {
        *nilmCloudFeature = NULL;
        return;
    }
    *nilmCloudFeature = gNilmCloudFeature + channel;
}

void setMinEventDeltaPower(int channel, float minEventDeltaPower) {
    if (minEventDeltaPower <= 0)
        return;
    gMinEventDeltaPower[channel] = minEventDeltaPower;
}

float getPowerCost(int channel) {
    return gPowerCost[channel];
}

int getAlgoVersion(void) {
    return VERSION[0] << 24 | VERSION[1] << 16 | VERSION[2] << 8 | VERSION[3];
}

// 过期判断
// appBuildDate sample "Mar 03 2020" 集成编译库文件的时间
// libBuildDate 生成库文件的时间
// expiredDate 认为指定的过期时间
#define STR_SIZE 11
static int gAppBuildYear = 2020, gAppBuildMonth = 4, gAppBuildDay = 2;
static int gLibBuildYear = 2020, gLibBuildMonth = 4, gLibBuildDay = 1;
static int isExpired(const char *appBuildDate, const char *libBuildDate, const int *expiredDate) {
    if (appBuildDate == NULL || libBuildDate == NULL || expiredDate == NULL)
        return 1;

    const static char MONTH_NAME[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
            "Oct", "Nov", "Dec" };
    char monthString[4] = { 0 };
    //解析prebuiltDate
    sscanf(appBuildDate, "%s %d %d", monthString, &gAppBuildDay, &gAppBuildYear);
    //防止时间是中文系统
    if (monthString[0] > 'Z' || monthString[0] < 'A')
        return 1;

    for (int i = 0; i < 12; i++) {
        if (strncasecmp(monthString, MONTH_NAME[i], 3) == 0) {
            gAppBuildMonth = i + 1;
            break;
        }
    }
    //解析startDate
    sscanf(libBuildDate, "%s %d %d", monthString, &gLibBuildDay, &gLibBuildYear);
    //防止时间是中文系统
    if (monthString[0] > 'Z' || monthString[0] < 'A')
        return 1;

    for (int i = 0; i < 12; i++) {
        if (strncasecmp(monthString, MONTH_NAME[i], 3) == 0) {
            gLibBuildMonth = i + 1;
            break;
        }
    }

    //const static int STR_SIZE = sizeof("2020-01-30");
    char appBuildDateString[STR_SIZE];
    memset(appBuildDateString, 0, sizeof(appBuildDateString));

    char libBuildDateString[STR_SIZE];
    memset(libBuildDateString, 0, sizeof(libBuildDateString));

    char expiredDateString[STR_SIZE];
    memset(expiredDateString, 0, sizeof(expiredDateString));

    sprintf(appBuildDateString, "%04d-%02d-%02d", gAppBuildYear, gAppBuildMonth, gAppBuildDay);
    sprintf(libBuildDateString, "%04d-%02d-%02d", gLibBuildYear, gLibBuildMonth, gLibBuildDay);
    sprintf(expiredDateString, "%04d-%02d-%02d", expiredDate[0], expiredDate[1], expiredDate[2]);

    if (strcmp(libBuildDateString, appBuildDateString) <= 0
            && strcmp(expiredDateString, appBuildDateString) >= 0) {
        return 0;
    }
    return 1;
}

static float gFft[128] = { 0 }; //全局变量,大内存全局变量防止栈溢出
#ifdef ARM_MATH_CM4
static float gArmFftinput[2 * 256] = { 0 };
#endif
static void getOddFft(float *cur, int curStart, float oddFft[5]) {
#ifdef ARM_MATH_CM4
    arm_cfft_radix2_instance_f32 S;
    int ifftFlag = 0;
    float *in = cur + curStart;
    memset(gArmFftinput, 0, sizeof(gArmFftinput));
    for (int i = 0; i < 128; i++) {
        gArmFftinput[2 * i] = in[i];
    }
    arm_cfft_radix2_init_f32(&S, 128, ifftFlag, 1);
    arm_cfft_radix2_f32(&S, gArmFftinput);
    arm_cmplx_mag_f32(gArmFftinput, gFft, 128);

    for (int j = 0; j < 5; j++) {
        oddFft[j] = gFft[j * 2 + 1] / 64;
    }
    return;
#else
    do_fft(cur + curStart, gFft);

    for (int j = 0; j < 5; j++) {
        oddFft[j] = gFft[j * 2 + 1];
    }
#endif
}

/**
 * 统计最大/最小的前几个数
 * 排序算法运算效率较低
 */
static void getTopDownAverage(float *data, int len, float *outTop, float *outDown) {

    float top[5] = { 0 }, down[5] = { 0 };
    float topAudit = top[0];
    float downAudit = down[0];
    float extreme = 0;
    char replaceFlag = 0;
    for (int i = 0; i < len; i++) {
        if (data[i] > topAudit) {
            extreme = 5000;
            replaceFlag = 0;
            for (int j = 0; j < 5; j++) {
                if (replaceFlag == 0 && topAudit == top[j]) {
                    top[j] = data[i]; //替换
                    replaceFlag = 1;
                }
                if (top[j] < extreme) { //找最小值
                    extreme = top[j];
                }
            }
            topAudit = extreme;
        } else if (data[i] < downAudit) {
            extreme = -5000;
            replaceFlag = 0;
            for (int j = 0; j < 5; j++) {
                if (replaceFlag == 0 && downAudit == down[j]) {
                    down[j] = data[i]; //替换
                    replaceFlag = 1;
                }
                if (down[j] > extreme) { //找最大值
                    extreme = down[j];
                }
            }
            downAudit = extreme;
        }
    }

    float topTmp = 0, downTmp = 0;
    for (int i = 0; i < 5; i++) {
        *outTop += top[i];
        if (top[i] > topTmp) {
            topTmp = top[i];
        }
        *outDown += down[i];
        if (down[i] < downTmp) {
            downTmp = down[i];
        }
    }
    *outTop = (*outTop - topTmp) / 4; //去除最大值
    *outDown = (*outDown - downTmp) / 4; //去除最小值
}

//计算电压畸变率
static float getAberrationRate(float *cur, int curStart) {
#ifdef ARM_MATH_CM4
    arm_cfft_radix2_instance_f32 S;
    int ifftFlag = 0;
    float *in = cur + curStart;
    memset(gArmFftinput, 0, sizeof(gArmFftinput));
    for (int i = 0; i < 128; i++) {
        gArmFftinput[2 * i] = in[i];
    }
    arm_cfft_radix2_init_f32(&S, 128, ifftFlag, 1);
    arm_cfft_radix2_f32(&S, gArmFftinput);
    arm_cmplx_mag_f32(gArmFftinput, gFft, 128);

    float squareSum = 0;
    for (int j = 2; j < 10; j++) {
        squareSum += (gFft[j] / 64) * (gFft[j] / 64);
    }
    if (gFft[1] == 0)
        return 0;
    squareSum = squareSum / (gFft[1] / 64) / (gFft[1] / 64);
    return sqrt(squareSum) * 100;
//    for (int j = 0; j < 5; j++) {
//        oddFft[j] = gFft[j * 2 + 1] / 64;
//    }

#else
    do_fft(cur + curStart, gFft);

//    for (int j = 0; j < 5; j++) {
//        oddFft[j] = gFft[j * 2 + 1];
//    }
    float squareSum = 0;
    for (int j = 2; j < 10; j++) { //取10次约等于31次的,减少计算量
        squareSum += gFft[j] * gFft[j];
    }
    if (gFft[1] == 0)
        return 0;
    squareSum = squareSum / (gFft[1] * gFft[1]);
    return sqrt(squareSum) * 100;
#endif
}

//电流波形分析
#define BATCH 4
static int getCurrentWaveFeature(float *cur, int curStart, float *vol, int volStart, float effI, float effU,
        WaveFeature *wf) {

    if (wf == NULL || cur == NULL || vol == NULL)
        return -1;
    // 窗口均值滤波
    float max = -999, min = 999;
    int maxIndex = 0, minIndex = 0;
    float c[33]; // 扩充1位
    memset(&c, 0, sizeof(c));
    for (int i = 0; i < 128; i += BATCH) {
        for (int j = 0; j < BATCH; j++) {
            c[i / BATCH] += cur[curStart + i + j] / BATCH;
        }
        if (c[i / BATCH] > max) {
            max = c[i / BATCH];
            maxIndex = i / BATCH;
        } else if (c[i / BATCH] < min) {
            min = c[i / BATCH];
            minIndex = i / BATCH;
        }
    }
    c[32] = c[0];
    float ad = 0, delta, average = 0;
    for (int i = 1; i < 33; i++) {
        delta = (float) (c[i] - c[i - 1]);
        delta = delta > 0 ? delta : -delta;
        ad += delta;
    }
    ad /= 32;

    for (int i = 0; i < 32; i++) {
        average += c[i];
    }
    average /= 32;

    int direction = 0, extremeNum = 0, flatNum = 0, flatBitmap = 0;
    float valueThresh = max / 9, deltaThresh = ad / 2, extremeThresh = max / 4;
    for (int i = 0; i < 32; i++) {
        // 周边至少一个点也是平肩
        if ((i >= 1 && fabs(c[i]) < valueThresh && fabs(c[i - 1]) < valueThresh
                && fabs(c[i] - c[i - 1]) < deltaThresh)
                || (i <= 31 && fabs(c[i]) < valueThresh && fabs(c[i + 1]) < valueThresh
                        && fabs(c[i + 1] - c[i]) < deltaThresh)) {
            flatNum++;
            flatBitmap = flatBitmap | (0x1 << i);
#if LOG_ON == 1
            printf("%d ", i);
#endif
        }
    }
#if LOG_ON == 1
    printf("\n");
#endif
    // 极值点个数，极值点非平肩
    for (int i = 0; i < 32; i++) {
        if (c[i + 1] - c[i] > 0 && ((flatBitmap & (0x1 << (i + 1))) == 0 || (flatBitmap & (0x1 << i)) == 0)) {
            if (direction < 0 && fabs(c[i]) > extremeThresh) {
                extremeNum++;
#if LOG_ON == 1
                printf("%d ", i);
#endif
            }
            direction = 1;
        } else if (c[i + 1] - c[i] < 0
                && ((flatBitmap & (0x1 << (i + 1))) == 0 || (flatBitmap & (0x1 << i)) == 0)) {
            if (direction > 0 && fabs(c[i]) > extremeThresh) {
                extremeNum++;
#if LOG_ON == 1
                printf("%d ", i);
#endif
            }
            direction = -1;
        }
    }
#if LOG_ON == 1
    printf("\n");
    for (int i = 0; i < 32; i++) {
        printf("%.2f\t", c[i]);
    }
    printf("\n");
#endif
    // 32点 - 极值点左右点数
    int maxLeftNum = 0, maxRightNum = 0, minLeftNum = 0, minRightNum = 0;
    for (int i = maxIndex - 1; i >= 0; i--) {
        if ((flatBitmap & (0x1 << i)) == 0) {
            maxLeftNum++;
        } else {
            break;
        }
    }
    for (int i = maxIndex + 1; i < 32; i++) {
        if ((flatBitmap & (0x1 << i)) == 0) {
            maxRightNum++;
        } else {
            break;
        }
    }
    for (int i = minIndex - 1; i >= 0; i--) {
        if ((flatBitmap & (0x1 << i)) == 0) {
            minLeftNum++;
        } else {
            break;
        }
    }
    for (int i = minIndex + 1; i < 32; i++) {
        if ((flatBitmap & (0x1 << i)) == 0) {
            minRightNum++;
        } else {
            break;
        }
    }

    //最大跳变和最大值
    float maxDelta = 0, maxValue = 0, minNegDelta = 0, minNegValue = 0;
    char maxPointIndex = 0, minPointIndex = 0;
    for (int i = curStart; i < curStart + 127; i++) {
        float delta = cur[i + 1] - cur[i];
        if (delta > maxDelta) {
            maxDelta = delta;
        } else if (delta < minNegDelta) {
            minNegDelta = delta;
        }
        if (cur[i] > maxValue) {
            maxValue = cur[i];
            maxPointIndex = i;
        } else if (cur[i] < minNegValue) {
            minNegValue = cur[i];
            minPointIndex = i;
        }
    }

    // 128点 - 极值点左右点数
    int maxLeftPointNum = 0, maxRightPointNum = 0, minLeftPointNum = 0, minRightPointNum = 0;
    static const float R1 = 0.12f;
    for (int i = maxPointIndex - 1; i >= 0; i--) {
        if (cur[i] > maxValue * R1) {
            maxLeftPointNum++;
        } else {
            break;
        }
    }
    for (int i = maxPointIndex + 1; i < 128; i++) {
        if (cur[i] > maxValue * R1) {
            maxRightPointNum++;
        } else {
            break;
        }
    }

    for (int i = minPointIndex - 1; i >= 0; i--) {
        if (cur[i] < minNegValue * R1) {
            minLeftPointNum++;
        } else {
            break;
        }
    }
    for (int i = minPointIndex + 1; i < 128; i++) {
        if (cur[i] < minNegValue * R1) {
            minRightPointNum++;
        } else {
            break;
        }
    }

    wf->extremeNum = extremeNum;
    wf->flatNum = flatNum;
    wf->maxLeftNum = maxLeftNum;
    wf->maxRightNum = maxRightNum;
    wf->minLeftNum = minLeftNum;
    wf->minRightNum = minRightNum;
    wf->maxLeftPointNum = maxLeftPointNum;
    wf->maxRightPointNum = maxRightPointNum;
    wf->minLeftPointNum = minLeftPointNum;
    wf->minRightPointNum = minRightPointNum;
    wf->maxDelta = maxDelta;
    wf->maxValue = maxValue;
    wf->minNegDelta = minNegDelta;
    wf->minNegValue = minNegValue;
    return 0;
}

//input data
int feedData(int channel, float *cur, float *vol, int unixTimestamp, char *extraMsg) {
    //step: 基本运算
    gTimer[channel]++;
    if (cur == NULL || vol == NULL) {
        return -1;
    }
    //报警状态或输出变量复位
    gChargingAlarm[channel] = 0;
    gDormConverterAlarm[channel] = 0;
    gMaliLoadAlarm[channel] = 0;
    gArcfaultAlarm[channel] = 0;
    gArcNum[channel] = 0;
    gThisPeriodNum[channel] = 0;
    memset(gNilmCloudFeature + channel, 0, sizeof(NilmCloudFeature));

    DateStruct ds;
    getDateByTimestamp(unixTimestamp, &ds);
    float effI = getEffectiveValue(cur, 0, 128); //当前有效电流
    float effU = getEffectiveValue(vol, 0, 128); //当前有效电压
    float activePower = getActivePower(cur, 0, vol, 0, 128); // 当前有功功率
    float reactivePower = getReactivePower(activePower, effI * effU);

    //更新能耗
    int timeDelta = unixTimestamp - gLastPowercostUpdateTime[channel];
    if (timeDelta > 0 && timeDelta <= 30) { //限定为(0,30s],防止时间错乱带来的重大影响
        gPowerCost[channel] += gLastActivePower[channel] / 1000 * timeDelta;
        gLastPowercostUpdateTime[channel] = unixTimestamp;
    } else if (timeDelta > 30) {
        gLastPowercostUpdateTime[channel] = unixTimestamp;
    }

    //将新波形插入buff
    insertFloatArrayToBuff(gIWaveBuff[channel], WAVE_BUFF_NUM, cur, 128);
    insertFloatArrayToBuff(gUWaveBuff[channel], WAVE_BUFF_NUM, vol, 128);
    insertFloatToBuff(gActivePBuff[channel], EFF_BUFF_NUM, activePower);

    float curSamplePosTop = 0, curSampleNegTop = 0;
    getTopDownAverage(cur, 128, &curSamplePosTop, &curSampleNegTop);

    //将功率插入10s短buff
    if (gTimer[channel] % 50 == 0) {
        PowerTrack powerTrack;
        powerTrack.activePower = activePower;
        powerTrack.reactivePower = reactivePower;
        powerTrack.sampleTime = unixTimestamp;
        powerTrack.totalPowerCost = 0;

        insertPackedDataToBuff((char*) gShortPowerTrack[channel], sizeof(gShortPowerTrack[channel]),
                (char*) (&powerTrack), sizeof(powerTrack));
        if (gShortTrackNum[channel] < SHORT_TRACK_SIZE) {
            gShortTrackNum[channel]++;
        }
        if (gTimer[channel] % 500 == 0) {
            insertPackedDataToBuff((char*) gPowerTrack[channel], sizeof(gPowerTrack[channel]),
                    (char*) (&powerTrack), sizeof(powerTrack));
            if (gPowerTrackNum[channel] < POWER_TRACK_SIZE) {
                gPowerTrackNum[channel]++;
            }
        }
    }

    //step: 事件监测
    int isStable = isPowerStable(gActivePBuff[channel], EFF_BUFF_NUM, 15, 2); //稳态判断
//#if TUOQIANG_DEBUG
//#if OUTLOG_ON
//    if (outprintf != NULL && isStable == 0 && gTimer[channel] % 50 == 0) {
//        outprintf("power\t%.1f \tI\t%.1f \tU\t%.1f\r\n", activePower, effI, effU);
//    }
//#endif
//#endif
    //负荷投切事件
    int switchEventHappen = 0;
    float absDeltaStandardPower = fabs(
            getStandardPower(activePower, effU)
                    - getStandardPower(gLastStableActivePower[channel], gLastStableUEff[channel]));
    if (isStable > gLastStable[channel]) {
        //投切事件发生
        if (absDeltaStandardPower > gMinEventDeltaPower[channel]) {
            switchEventHappen = 1;
        }
    }
    if (isStable != gLastStable[channel] && absDeltaStandardPower > gMinEventDeltaPower[channel] / 2) {
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("[%d]gt=%d st=%d ap=%.1f rp=%.1f se=%d\r\n", channel, gTimer[channel], isStable,
                    activePower, getReactivePower(activePower, effI * effU), switchEventHappen);
        }
#endif
    }
    //负荷有功/无功缓慢上升事件
    int apRiseCounter = 0, rpRiseCounter = 0;
    if (gShortTrackNum[channel] >= SHORT_TRACK_SIZE) {
        for (int i = 0; i < SHORT_TRACK_SIZE - 1; i++) {
            if (gShortPowerTrack[channel][i].activePower < gShortPowerTrack[channel][i + 1].activePower) {
                apRiseCounter++;
            } else {
                apRiseCounter = 0;
            }
            if (gShortPowerTrack[channel][i].reactivePower < gShortPowerTrack[channel][i + 1].reactivePower) {
                rpRiseCounter++;
            } else {
                rpRiseCounter = 0;
            }
        }
    }
//#if TUOQIANG_DEBUG
//#if OUTLOG_ON
//    static int lastApRiseCounter = 0;
//    if (apRiseCounter >= 2 && apRiseCounter != lastApRiseCounter) {
//        outprintf("gt=%d ut=%d aprc=%d rprc=%d ap=%.1f rp=%.1f\r\n", gTimer[channel], unixTimestamp % 1000,
//                apRiseCounter, rpRiseCounter, activePower, reactivePower);
//    }
//    lastApRiseCounter = apRiseCounter;
//#endif
//#endif
    //step:特征提取
    float deltaEffI = LF, deltaEffU = LF, iPulse = LF, deltaActivePower = LF, deltaReactivePower = LF,
            curSamplePulse = LF; //斩波调节时没有脉冲,curSample可能小于1
    float deltaOddFft[5] = { LF, LF, LF, LF, LF }; //奇次谐波
    int startupTime = 0; // 启动时间
    int zeroCrossLast = -1, zeroCrossThis = -1; //上个周期及本个周期稳态电压穿越

    static float voltageAberrRate[CHANNEL_NUM] = { 0 };
    //每半小时更新一次电压畸变率，节省性能. 如果持续为0，则每秒更新1次
    if (gFuncMaliciousLoadEnabled
            && (gTimer[channel] % 90000 == 10
                    || (voltageAberrRate[channel] < LF && gTimer[channel] % 50 == 10))) {
        voltageAberrRate[channel] = getAberrationRate(vol, 0);
        if (voltageAberrRate[channel] > 9)
            voltageAberrRate[channel] = 9;
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("[%d]var=%.1f\r\n", channel, voltageAberrRate[channel]);
            if (voltageAberrRate[channel] < 0.1f) {
                outprintf("[%d]vol:%.1f %.1f %.1f %.1f\r\n", channel, vol[0], vol[1], vol[2], vol[3]);
            }
        }
#endif
    }
    WaveFeature deltaWf;
    memset(&deltaWf, 0, sizeof(deltaWf));
    if (switchEventHappen) {
        // 计算差分电流
        zeroCrossLast = getZeroCrossIndex(gLastStableUWaveBuff[channel], 0, 256);
        zeroCrossThis = getZeroCrossIndex(gUWaveBuff[channel], 0, 256);

        for (int i = 0; i < 128; i++) {
            gDeltaIWaveBuff[channel][i] = gIWaveBuff[channel][zeroCrossThis + i]
                    - gLastStableIWaveBuff[channel][zeroCrossLast + i];
        }

        // feature1: fft
        getOddFft(gDeltaIWaveBuff[channel], 0, deltaOddFft);
        // feature2:
        deltaEffI = getEffectiveValue(gDeltaIWaveBuff[channel], 0, 128);
        deltaEffU = getEffectiveValue(gUWaveBuff[channel], zeroCrossThis, 128);
        deltaActivePower = getActivePower(gDeltaIWaveBuff[channel], 0, gUWaveBuff[channel], zeroCrossThis,
                128);
        deltaReactivePower = getReactivePower(deltaActivePower, deltaEffI * deltaEffU);

        // feature3:
        startupTime = gTransitTime[channel] - EFF_BUFF_NUM;
        startupTime = startupTime < 0 ? 0 : startupTime;

        // feature4:
        iPulse = (gTransitTmpIMax[channel] - gLastStableIEff[channel]) / (effI - gLastStableIEff[channel]);

        //过渡期间的采样值的脉冲比例
        float curSamplePulsePos = (gTransitCurSamplePosTopMax[channel] - gLastStableCurSamplePosTop[channel])
                / (curSamplePosTop - gLastStableCurSamplePosTop[channel]);
        float curSamplePulseNeg = (gTransitCurSampleNegTopMax[channel] - gLastStableCurSampleNegTop[channel])
                / (curSampleNegTop - gLastStableCurSampleNegTop[channel]);
        curSamplePulse = curSamplePulsePos > curSamplePulseNeg ? curSamplePulsePos : curSamplePulseNeg;

        gLastProcessedStableActivePower[channel] = activePower; //稳态功率缓慢上升的场景,processed不会变

        getCurrentWaveFeature(gDeltaIWaveBuff[channel], 0, gUWaveBuff[channel], zeroCrossThis, deltaEffI,
                deltaEffU, &deltaWf);

#if LOG_ON == 1
        for (int i = 0; i < 128; i++) {
            printf("%.2f\t", gDeltaIWaveBuff[channel][i]);
        }
        if (deltaActivePower > 0) {
            printf("\n");
            printf(
                    "ext=%d flat=%d iPulse=%.2f csp=%.2f str=%d lpap=%.2f dap=%.2f drp=%.2f fft=[%.2f %.2f %.2f %.2f %.2f]\n",
                    deltaWf.extremeNum, deltaWf.flatNum, iPulse, curSamplePulse, startupTime,
                    gLastProcessedStableActivePower[channel], deltaActivePower, deltaReactivePower,
                    deltaOddFft[0], deltaOddFft[1], deltaOddFft[2], deltaOddFft[3], deltaOddFft[4]);
        }
#endif
    }

    //step:业务处理
    //斩波式宿舍调压器检测
    //连续5秒有功和无功都增长
    int dormConverterAlarmTimeDelta = unixTimestamp - gDormConverterLastAlarmTime[channel];
    if (dormConverterAlarmTimeDelta < 0) { //时间保护
        gDormConverterLastAlarmTime[channel] = 0;
    }
    if (gFuncDormConverterEnabled) {

        if (apRiseCounter >= 5 && rpRiseCounter >= 5 && dormConverterAlarmTimeDelta >= 10) {
            int zeroCross = getZeroCrossIndex(gUWaveBuff[channel], 0, 256);
            WaveFeature cwf;
            getCurrentWaveFeature(gIWaveBuff[channel], zeroCross, gUWaveBuff[channel], zeroCross, effI, effU,
                    &cwf);
            gDormConverterAlarm[channel] = dormConverterAdjustingCheck(channel, activePower, reactivePower,
                    &cwf,
                    NULL);
            if (gDormConverterAlarm[channel]) {
#if OUTLOG_ON
                if (outprintf != NULL) {
                    outprintf("[%d]DormConverter adjusting detected\r\n", channel);
                }
#endif
                gDormConverterLastAlarmTime[channel] = unixTimestamp;
            }
        }

        if (switchEventHappen && !gDormConverterAlarm[channel] && dormConverterAlarmTimeDelta >= 10) {
            gDormConverterAlarm[channel] = dormConverterDetect(channel, deltaActivePower, deltaReactivePower,
                    &deltaWf,
                    NULL);
            if (gDormConverterAlarm[channel]) {
#if OUTLOG_ON
                if (outprintf != NULL) {
                    outprintf("[%d]DormConverter detected\r\n", channel);
                }
#endif
                gDormConverterLastAlarmTime[channel] = unixTimestamp;
            }
        }
    }

    //充电检测
    if (gFuncChargingAlarmEnabled && switchEventHappen) {
        gChargingAlarm[channel] = chargingDetect(channel, deltaOddFft, iPulse, curSamplePulse,
                deltaActivePower, deltaReactivePower, startupTime, &deltaWf, NULL);
        if (gChargingAlarm[channel]) {
#if OUTLOG_ON
            if (outprintf != NULL) {
                outprintf("[%d]Charging detected\r\n", channel);
            }
#endif
        }
    }

    //恶性负载识别
    if (gFuncMaliciousLoadEnabled && switchEventHappen) {

        char msg[50] = { 0 };
        gMaliLoadAlarm[channel] = maliciousLoadDetect(channel, deltaOddFft, iPulse, curSamplePulse,
                deltaActivePower, deltaReactivePower, effU, activePower, reactivePower,
                voltageAberrRate[channel], &deltaWf, &ds, msg);
#if TUOQIANG_DEBUG
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("\t%.2f\t%.2f\t%.2f da\t%.2f\t%.2f\r\n", deltaOddFft[0], deltaOddFft[1], deltaOddFft[2],
                    deltaActivePower, deltaReactivePower);
        }
#endif
#endif
//        if (strlen(msg) > 0)
//            strcpy(extraMsg,msg);
#if OUTLOG_ON
        if (outprintf != NULL && gMaliLoadAlarm[channel]) {
            outprintf("[%d]MaliLoadAlarm detected\r\n", channel);
        }
#endif
    }

    //故障电弧
    if (gFuncArcfaultEnabled) {
        // float oddFft[5] = { 0 };
        // getOddFft(cur, 0, oddFft);
        gArcfaultAlarm[channel] = arcfaultDetect(channel, unixTimestamp, &ds, cur, vol, effI, NULL,
                &gArcNum[channel], &gThisPeriodNum[channel], NULL);

        static char lastArcFaultAlarm[CHANNEL_NUM] = { 0 };
        if (gArcfaultAlarm[channel] && dormConverterAlarmTimeDelta <= 20) { //调压器与故障电弧互斥,发现调压器的10s内不再报电弧
#if OUTLOG_ON

            if (outprintf != NULL && gArcfaultAlarm[channel] > lastArcFaultAlarm[channel]) {
                outprintf("[%d]force ignore arcalarm for dc\r\n", channel);
            }
#endif
            gArcfaultAlarm[channel] = 0;
        }

#if OUTLOG_ON
        if (outprintf != NULL && gArcfaultAlarm[channel] > lastArcFaultAlarm[channel]) {
            outprintf("[%d]ArcFault detected\r\n", channel);
        }
#endif
        lastArcFaultAlarm[channel] = gArcfaultAlarm[channel];
    }

//nilmCloudFeature
    if (gNilmCloudFeatureEnabled && switchEventHappen) {
        gNilmCloudFeature[channel].activePower = activePower;
        gNilmCloudFeature[channel].current = effI;
        gNilmCloudFeature[channel].voltage = effU;
        gNilmCloudFeature[channel].deltaActivePower = deltaActivePower;
        gNilmCloudFeature[channel].deltaCurrent = deltaEffI;
        gNilmCloudFeature[channel].iPulse = iPulse;
        gNilmCloudFeature[channel].startupTime = startupTime;
        gNilmCloudFeature[channel].fft1 = deltaOddFft[0];
        gNilmCloudFeature[channel].fft3 = deltaOddFft[1];
        gNilmCloudFeature[channel].fft5 = deltaOddFft[2];
        gNilmCloudFeature[channel].fft7 = deltaOddFft[3];
        gNilmCloudFeature[channel].fft9 = deltaOddFft[4];
        gNilmCloudFeature[channel].unixTime = unixTimestamp;
    }

//step:状态和变量更新
// 稳态窗口的变量赋值和状态刷新
    if (isStable > 0) {
        for (int i = 0; i < WAVE_BUFF_NUM; i++) {
            gLastStableIWaveBuff[channel][i] = gIWaveBuff[channel][i];
            gLastStableUWaveBuff[channel][i] = gUWaveBuff[channel][i];
        }
        gLastStableIEff[channel] = effI;
        gLastStableUEff[channel] = effU;
        gLastStableActivePower[channel] = activePower;
        gLastStableCurSamplePosTop[channel] = curSamplePosTop;
        gLastStableCurSampleNegTop[channel] = curSampleNegTop;
        gTransitTime[channel] = 0;
        gTransitTmpIMax[channel] = 0;
        gTransitCurSamplePosTopMax[channel] = 0;
        gTransitCurSampleNegTopMax[channel] = 0;
    } else { // 非稳态
        gTransitTime[channel]++;
        if (effI > gTransitTmpIMax[channel]) {
            gTransitTmpIMax[channel] = effI;
        }
        if (curSamplePosTop > gTransitCurSamplePosTopMax[channel]) {
            gTransitCurSamplePosTopMax[channel] = curSamplePosTop;
        }
        if (curSampleNegTop < gTransitCurSampleNegTopMax[channel]) {
            gTransitCurSampleNegTopMax[channel] = curSampleNegTop;
        }
    }

    gLastStable[channel] = isStable;
    gLastActivePower[channel] = activePower;

//注意:人为添加不利因素
    if (gIsLibExpired) {
        //对于过期库,添加随机干扰
        gChargingAlarm[channel] = 0;
        gDormConverterAlarm[channel] = 0;
        gMaliLoadAlarm[channel] = 0;
        gArcfaultAlarm[channel] = 0;
        return 2;
    }

//2020.11.30 2021.3.1 ok
//2020.11.30 2020.3.1 fail
    if (gLibBuildYear > ds.year || (gLibBuildYear == ds.year && gLibBuildMonth > ds.mon)
            || (gLibBuildYear == ds.year && gLibBuildMonth == ds.mon && gLibBuildDay > ds.mday)) {
        gChargingAlarm[channel] = 0;
        gDormConverterAlarm[channel] = 0;
        gMaliLoadAlarm[channel] = 0;
        gArcfaultAlarm[channel] = 0;
        return 3;
    }

    if (DEBUG_ONLY) {
        if (gTimer[channel] >= 50 * 86400 * 2) {
            gChargingAlarm[channel] = 0;
            gDormConverterAlarm[channel] = 0;
            gMaliLoadAlarm[channel] = 0;
            gArcfaultAlarm[channel] = 0;
            return 4;
        }
    }

    return 0;
}

extern char *APP_BUILD_DATE;
const char *PARTNER = "CHTQDQ";
int initTpsonAlgoLib(void) {
#if LOG_ON == 1
    printf("Released to %s at 2020 by TPSON. Integrated at %s.\n", PARTNER, APP_BUILD_DATE);
#endif
    char expiredDate[11];
    memset(expiredDate, 0, sizeof(expiredDate));
    gIsLibExpired = isExpired(APP_BUILD_DATE, __DATE__, B_MAX);

    memset(gActivePBuff, 0, sizeof(gActivePBuff));
    memset(gIWaveBuff, 0, sizeof(gActivePBuff));
    memset(gUWaveBuff, 0, sizeof(gUWaveBuff));
    memset(gLastStableIWaveBuff, 0, sizeof(gLastStableIWaveBuff));
    memset(gLastStableUWaveBuff, 0, sizeof(gLastStableUWaveBuff));
    memset(gLastStableIEff, 0, sizeof(gLastStableIEff));
    memset(gLastStableCurSamplePosTop, 0, sizeof(gLastStableCurSamplePosTop));
    memset(gLastStableCurSampleNegTop, 0, sizeof(gLastStableCurSampleNegTop));
    memset(gLastStableUEff, 0, sizeof(gLastStableUEff));
    memset(gDeltaIWaveBuff, 0, sizeof(gDeltaIWaveBuff));

    for (int i = 0; i < CHANNEL_NUM; i++) {
        gLastStable[i] = 1; // 上个周期的稳态情况
        gMinEventDeltaPower[i] = 90.0f;
    }

    memset(gTransitTime, 0, sizeof(gTransitTime)); // 负荷变化过渡/非稳态时间
    memset(gTransitTmpIMax, 0, sizeof(gTransitTmpIMax));
    memset(gTransitCurSamplePosTopMax, 0, sizeof(gTransitCurSamplePosTopMax));
    memset(gTransitCurSampleNegTopMax, 0, sizeof(gTransitCurSampleNegTopMax));

    memset(gLastStableActivePower, 0, sizeof(gLastStableActivePower)); //稳态窗口下最近的有功功率,对于缓慢变化的场景会跟随变化
    memset(gLastProcessedStableActivePower, 0, sizeof(gLastProcessedStableActivePower)); //上次经过稳态事件处理的有功功率,对于缓慢变化的场景不会跟随变化

    memset(gTimer, 0, sizeof(gTimer));

    memset(gChargingAlarm, 0, sizeof(gChargingAlarm));
    memset(gDormConverterAlarm, 0, sizeof(gDormConverterAlarm));
    memset(gMaliLoadAlarm, 0, sizeof(gMaliLoadAlarm));
    memset(gArcfaultAlarm, 0, sizeof(gArcfaultAlarm));
    memset(gDormConverterLastAlarmTime, 0, sizeof(gDormConverterLastAlarmTime));
    memset(gArcNum, 0, sizeof(gArcNum));
    memset(gThisPeriodNum, 0, sizeof(gThisPeriodNum));

    memset(gShortPowerTrack, 0, sizeof(gShortPowerTrack));
    memset(gShortTrackNum, 0, sizeof(gShortTrackNum));
    memset(gPowerTrack, 0, sizeof(gPowerTrack));
    memset(gPowerTrackNum, 0, sizeof(gPowerTrackNum));

    memset(gPowerCost, 0, sizeof(gPowerCost));
    memset(gLastPowercostUpdateTime, 0, sizeof(gLastPowercostUpdateTime));
    memset(gLastActivePower, 0, sizeof(gLastActivePower));

    memset(gNilmCloudFeature, 0, sizeof(gNilmCloudFeature));

//step:初始化算法模块
    initFuncArcfault();
    initFuncDormConverter();
    initFuncMaliLoad();
    initFuncChargingAlarm();
    if (gIsLibExpired)
        return -2;
#if OUTLOG_ON
    if (outprintf != NULL) {
        outprintf("init ok v%d\r\n", getAlgoVersion());
    }
#endif
    return 0;
}
