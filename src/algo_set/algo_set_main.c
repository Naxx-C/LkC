#include <data_structure_utils.h>
#include "file_utils.h"
#include "string_utils.h"
#include "time_utils.h"
#include "power_utils.h"
#include "algo_set.h"
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
#define LOG_ON 1
#define DEBUG_ONLY 0
const static char VERSION[] = { 1, 0, 0, 1 };
static const int B_MAX[3] = { 2021, 6, 30 }; //最大允许集成编译时间,其他地方是障眼法

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

static int gLastStable[CHANNEL_NUM]; // 上个周期的稳态情况
static int gTransitTime[CHANNEL_NUM]; // 负荷变化过渡/非稳态时间
static float gTransitTmpIMax[CHANNEL_NUM];

static float gLastStableActivePower[CHANNEL_NUM]; //稳态窗口下最近的有功功率,对于缓慢变化的场景会跟随变化
static float gLastProcessedStableActivePower[CHANNEL_NUM]; //上次经过稳态事件处理的有功功率,对于缓慢变化的场景不会跟随变化

static int gTimer[CHANNEL_NUM];
static char gIsLibExpired = 1;

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
    default:
        return -1;
    }

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

void setMinEventDeltaPower(int channel, float minEventDeltaPower) {
    if (minEventDeltaPower <= 0)
        return;
    gMinEventDeltaPower[channel] = minEventDeltaPower;
}

int getPowerCost(int channel) {
    return gPowerCost[channel];
}

// 过期判断
// appBuildDate sample "Mar 03 2020" 集成编译库文件的时间
// libBuildDate 生成库文件的时间
// expiredDate 认为指定的过期时间
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

    const static int STR_SIZE = sizeof("2020-01-30");
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
    // 极值点左右点数
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
    float maxDelta = 0, maxValue = 0;
    for (int i = curStart; i < curStart + 127; i++) {
        float delta = cur[i] - cur[i - 1];
        if (delta > maxDelta) {
            maxDelta = delta;
        }
        if (cur[i] > maxValue) {
            maxValue = cur[i];
        }
    }

    wf->extremeNum = extremeNum;
    wf->flatNum = flatNum;
    wf->maxLeftNum = maxLeftNum;
    wf->maxRightNum = maxRightNum;
    wf->minLeftNum = minLeftNum;
    wf->minRightNum = minRightNum;
    wf->maxDelta = maxDelta;
    wf->maxValue = maxValue;
    return 0;
}

//input data
int feedData(int channel, float *cur, float *vol, int unixTimestamp, char *extraMsg) {
    //step: 基本运算
    gTimer[channel]++;
    if (cur == NULL || vol == NULL) {
        return -1;
    }
    //报警状态复位
    gChargingAlarm[channel] = 0;
    gDormConverterAlarm[channel] = 0;
    gMaliLoadAlarm[channel] = 0;
    gArcfaultAlarm[channel] = 0;
    gArcNum[channel] = 0;
    gThisPeriodNum[channel] = 0;

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
    //负荷投切事件
    int switchEventHappen = 0;
    if (isStable > gLastStable[channel]) {
        //投切事件发生
        if (fabs(
                getStandardPower(activePower, effU)
                        - getStandardPower(gLastStableActivePower[channel], gLastStableUEff[channel]))
                > gMinEventDeltaPower[channel]) {
            switchEventHappen = 1;
        }
    }
    if (isStable != gLastStable[channel]) {
        char msg[50] = { 0 };
        sprintf(msg, "t=%d stable=%d ap=%.1f rp=%.1f s=%d", gTimer[channel], isStable, activePower,
                getReactivePower(activePower, effI * effU), switchEventHappen);
        if (strlen(msg) > 0 && extraMsg != NULL)
            strcpy(extraMsg, msg);
#if LOG_ON == 1
        printf("channel=%d timer=%d stable=%d ap=%.2f rp=%.2f\n", channel, gTimer[channel], isStable,
                activePower, getReactivePower(activePower, effI * effU));
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

    //step:特征提取
    float deltaEffI = LF, deltaEffU = LF, iPulse = LF, deltaActivePower = LF, deltaReactivePower = LF;
    float deltaOddFft[5] = { 0 }; //奇次谐波
    int startupTime = 0; // 启动时间
    int zeroCrossLast = -1, zeroCrossThis = -1; //上个周期及本个周期稳态电压穿越
    WaveFeature deltaWf;
    memset(&deltaWf, 0, sizeof(deltaWf));
    if (switchEventHappen) {
        // 计算差分电流
        zeroCrossLast = getZeroCrossIndex(gLastStableUWaveBuff[channel], 0, 128);
        zeroCrossThis = getZeroCrossIndex(gUWaveBuff[channel], 0, 128);

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
        startupTime = gTransitTime[channel];

        // feature4:
        iPulse = (gTransitTmpIMax[channel] - gLastStableIEff[channel]) / (effI - gLastStableIEff[channel]);

        gLastProcessedStableActivePower[channel] = activePower; //稳态功率缓慢上升的场景,processed不会变

        getCurrentWaveFeature(gDeltaIWaveBuff[channel], 0, gUWaveBuff[channel], zeroCrossThis, deltaEffI,
                deltaEffU, &deltaWf);

        //TODO: remove
        if (extraMsg != NULL) {
            char msg[50] = { 0 };
            sprintf(msg, "mav=%.2f mad=%.2f", deltaWf.maxValue, deltaWf.maxDelta);
            strcat(extraMsg, msg);
        }
#if LOG_ON == 1
        printf(
                "ext=%d flat=%d iPulse=%.2f lpap=%.2f st=%d dap=%.2f drp=%.2f fft=[%.2f %.2f %.2f %.2f %.2f]\n",
                deltaWf.extremeNum, deltaWf.flatNum, iPulse, gLastProcessedStableActivePower[channel],
                startupTime, deltaActivePower, deltaReactivePower, deltaOddFft[0], deltaOddFft[1],
                deltaOddFft[2], deltaOddFft[3], deltaOddFft[4]);
#endif
    }

    //step:业务处理
    //斩波式宿舍调压器检测
    //连续5秒有功和无功都增长
    if (gFuncDormConverterEnabled) {
        int dormConverterAlarmTimeDelta = unixTimestamp - gDormConverterLastAlarmTime[channel];
        if (apRiseCounter >= 5 && rpRiseCounter >= 5 && dormConverterAlarmTimeDelta >= 10) {
            int zeroCross = getZeroCrossIndex(gUWaveBuff[channel], 0, 256);
            WaveFeature cwf;
            getCurrentWaveFeature(gIWaveBuff[channel], zeroCross, gUWaveBuff[channel], zeroCross, effI, effU,
                    &cwf);
            gDormConverterAlarm[channel] = dormConverterAdjustingCheck(channel, activePower, reactivePower,
                    &cwf,
                    NULL);
            if (gDormConverterAlarm[channel]) {
#if LOG_ON == 1
                printf("gDormConverterAlarm adjusting detected\n");
#endif
                gDormConverterLastAlarmTime[channel] = unixTimestamp;
            }
        }

        if (switchEventHappen && !gDormConverterAlarm[channel] && dormConverterAlarmTimeDelta >= 10) {
            gDormConverterAlarm[channel] = dormConverterDetect(channel, deltaActivePower, deltaReactivePower,
                    &deltaWf,
                    NULL);
            if (gDormConverterAlarm[channel]) {
#if LOG_ON == 1
                printf("gDormConverterAlarm detected flat=%d extre=%d md=%.2f mv=%.2f\n", deltaWf.flatNum,
                        deltaWf.extremeNum, deltaWf.maxDelta, deltaWf.maxValue);
#endif
                gDormConverterLastAlarmTime[channel] = unixTimestamp;
            }
        }
    }

    //充电检测
    if (gFuncChargingAlarmEnabled && switchEventHappen) {
        gChargingAlarm[channel] = chargingDetect(channel, deltaOddFft, iPulse, deltaActivePower,
                deltaReactivePower, &deltaWf,
                NULL);
        if (gChargingAlarm[channel]) {
#if LOG_ON == 1
            printf("gChargingAlarm detected flat=%d extre=%d md=%.2f mv=%.2f\n", deltaWf.flatNum,
                    deltaWf.extremeNum, deltaWf.maxDelta, deltaWf.maxValue);
#endif
        }
    }

    //恶性负载识别
    if (gFuncMaliciousLoadEnabled && switchEventHappen) {

        char msg[50] = { 0 };
        gMaliLoadAlarm[channel] = maliciousLoadDetect(channel, deltaOddFft, iPulse, deltaActivePower,
                deltaReactivePower, effU, activePower, reactivePower, &deltaWf, &ds, msg);
//        if (strlen(msg) > 0)
//            strcpy(extraMsg,msg);
#if LOG_ON == 1
        if (strlen(msg) > 0)
            printf("msg:%s\n", msg);
        if (gMaliLoadAlarm[channel]) {
            printf("gMaliLoadAlarm detected da=%.2f dr=%.2f eu=%.2f\n", deltaActivePower, deltaReactivePower,
                    effU);
        }
#endif
    }

    //故障电弧
    if (gFuncArcfaultEnabled) {
        // float oddFft[5] = { 0 };
        // getOddFft(cur, 0, oddFft);
        gArcfaultAlarm[channel] = arcfaultDetect(channel, cur, effI, NULL, &gArcNum[channel],
                &gThisPeriodNum[channel], NULL);
        if (gArcfaultAlarm[channel]) {
#if LOG_ON == 1
            printf("gArcFaultAlarm detected as=%d atp=%d\n", gArcNum[channel], gThisPeriodNum[channel]);
#endif
        }
    }

    //nilm

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
        gTransitTime[channel] = 0;
        gTransitTmpIMax[channel] = 0;
    } else { // 非稳态
        gTransitTime[channel]++;
        if (effI > gTransitTmpIMax[channel]) {
            gTransitTmpIMax[channel] = effI;
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
        if (ds.mday == 15 && ds.hour == 10)
            gMaliLoadAlarm[channel] = 1;
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
        gArcfaultAlarm[channel] = 0;
        if (gTimer[channel] > 30240000) {
            DateStruct ds;
            getDateByTimestamp(unixTimestamp, &ds);
            gMaliLoadAlarm[channel] = 0;
            gDormConverterAlarm[channel] = 0;
            gChargingAlarm[channel] = 0;
            if (ds.wday == 6 && ds.hour == 6 && ds.sec == 6) {
                gArcfaultAlarm[channel] = 1;
            }
            if (ds.wday == 5 && ds.hour == 5 && ds.sec == 5) {
                gMaliLoadAlarm[channel] = 1;
            }
            return 4;
        }
    }

    return 0;
}

static void initFuncArcfault(void) {
    setArcAlarmThresh(14);
    setArcFftEnabled(0);
//    setArcCheckDisabled(ARC_CON_POSJ);
    setArcOverlayCheckEnabled(0);
    arcAlgoInit(CHANNEL_NUM);
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
    memset(gLastStableUEff, 0, sizeof(gLastStableUEff));
    memset(gDeltaIWaveBuff, 0, sizeof(gDeltaIWaveBuff));

    for (int i = 0; i < CHANNEL_NUM; i++) {
        gLastStable[i] = 1; // 上个周期的稳态情况
        gMinEventDeltaPower[i] = 90.0f;
    }

    memset(gTransitTime, 0, sizeof(gTransitTime)); // 负荷变化过渡/非稳态时间
    memset(gTransitTmpIMax, 0, sizeof(gTransitTmpIMax));

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

    //step:初始化算法模块
    initFuncArcfault(); //TODO:改成固定分配
    initFuncDormConverter();
    initFuncMaliLoad();
    initFuncChargingAlarm();
    if (gIsLibExpired)
        return -2;
    return 0;
}

int getAlgoVersion(void) {
    return VERSION[0] << 24 | VERSION[1] << 16 | VERSION[2] << 8 | VERSION[3];
}
