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
#include <stdio.h>
#include <sys/stat.h>
#include "dirent.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef ARM_MATH_CM4
#include "arm_math.h"
#else
#include <math.h>
#include "fft.h"
#endif

/**固定定义区*/
#define LOG_ON 1
#define PI 3.14159265f
const static char VERSION[] = { 1, 0, 0, 1 };
static const char *EXPIRED_DATE = "2021-06-30";

/**运行变量区*/
#define WAVE_BUFF_NUM 512
#define EFF_BUFF_NUM (50 * 1) // effective current buffer NUM
static float gActivePBuff[EFF_BUFF_NUM];
static float gIWaveBuff[WAVE_BUFF_NUM]; // FIFO buff, pos0是最老的数据
static float gUWaveBuff[WAVE_BUFF_NUM];
static float gLastStableIWaveBuff[WAVE_BUFF_NUM];
static float gLastStableUWaveBuff[WAVE_BUFF_NUM];
static float gLastStableIEff = 0;
static float gLastStableUEff = 0;
static float gDeltaIWaveBuff[WAVE_BUFF_NUM];

static int gLastStable = 1; // 上个周期的稳态情况
static int gTransitTime = 0; // 负荷变化过渡/非稳态时间
static float gTransitTmpIMax = 0;

static float gLastStableActivePower = 0; //稳态窗口下最近的有功功率,对于缓慢变化的场景会跟随变化
static float gLastProcessedStableActivePower = 0; //上次经过稳态事件处理的有功功率,对于缓慢变化的场景不会跟随变化

static int gTimer = 0;
static char gIsLibExpired = 1;

static int gChargingAlarm = 0;
static int gDormConverterAlarm = 0;
static int gDormConverterLastAlarmTime = 0;
static int gMaliLoadAlarm = 0;

#define SHORT_TRACK_SIZE 10 // 1s采样一次，10s数据
static PowerTrack gShortPowerTrack[SHORT_TRACK_SIZE]; //存储电能足迹
static int gShortTrackNum = 0;

#define POWER_TRACK_SIZE (60*6) // 10s采样一次，60min数据
static PowerTrack gPowerTrack[POWER_TRACK_SIZE]; //存储电能足迹
static int gPowerTrackNum = 0;

static float gPowerCost = 0; //时间内线路总功耗kws
static int gLastPowercostUpdateTime = 0; //上一次更新功耗时的线路总的有功功率
static float gLastActivePower = 0;

/**可配变量区*/
static float gMinEventDeltaPower = 90.0f;

void setMinEventDeltaPower(float minEventDeltaPower) {
    gMinEventDeltaPower = minEventDeltaPower;
}

// 过期判断
// prebuiltDate sample "Mar 03 2020" 集成编译库文件的时间
// startDate 生成库文件的时间
// expiredDate 认为指定的过期时间
// TODO:只接受startDate到expiredDate范围内的编译
static int isExpired(const char *prebuiltDate, const char *startDate, const char *expiredDate) {
    if (prebuiltDate == NULL || startDate == NULL || expiredDate == NULL)
        return 1;

    const static char MONTH_NAME[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
            "Oct", "Nov", "Dec" };
    char monthString[4] = { 0 };
    int prebuiltYear = 2020, prebuiltMonth = 4, prebuiltDay = 2;
    int startDateYear = 2020, startDateMonth = 4, startDateDay = 1;
    //解析prebuiltDate
    sscanf(prebuiltDate, "%s %d %d", monthString, &prebuiltDay, &prebuiltYear);
    //防止时间是中文系统
    if (monthString[0] > 'Z' || monthString[0] < 'A')
        return 1;

    for (int i = 0; i < 12; i++) {
        if (strncasecmp(monthString, MONTH_NAME[i], 3) == 0) {
            prebuiltMonth = i + 1;
            break;
        }
    }
    //解析startDate
    sscanf(startDate, "%s %d %d", monthString, &startDateDay, &startDateYear);
    //防止时间是中文系统
    if (monthString[0] > 'Z' || monthString[0] < 'A')
        return 1;

    for (int i = 0; i < 12; i++) {
        if (strncasecmp(monthString, MONTH_NAME[i], 3) == 0) {
            startDateMonth = i + 1;
            break;
        }
    }

    const static int STR_SIZE = sizeof("2021-06-30");
    char preBuiltDateString[STR_SIZE];
    memset(preBuiltDateString, 0, sizeof(preBuiltDateString));

    char startDateString[STR_SIZE];
    memset(startDateString, 0, sizeof(startDateString));

    sprintf(preBuiltDateString, "%04d-%02d-%02d", prebuiltYear, prebuiltMonth, prebuiltDay);
    sprintf(startDateString, "%04d-%02d-%02d", startDateYear, startDateMonth, startDateDay);

    if (strcmp(startDateString, preBuiltDateString) <= 0 && strcmp(EXPIRED_DATE, preBuiltDateString) >= 0) {
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
            if (LOG_ON) {
                printf("%d ", i);
            }
        }
    }
    if (LOG_ON) {
        printf("\n");
    }
    // 极值点个数，极值点非平肩
    for (int i = 0; i < 32; i++) {
        if (c[i + 1] - c[i] > 0 && ((flatBitmap & (0x1 << (i + 1))) == 0 || (flatBitmap & (0x1 << i)) == 0)) {
            if (direction < 0 && fabs(c[i]) > extremeThresh) {
                extremeNum++;
                if (LOG_ON) {
                    printf("%d ", i);
                }
            }
            direction = 1;
        } else if (c[i + 1] - c[i] < 0
                && ((flatBitmap & (0x1 << (i + 1))) == 0 || (flatBitmap & (0x1 << i)) == 0)) {
            if (direction > 0 && fabs(c[i]) > extremeThresh) {
                extremeNum++;
                if (LOG_ON) {
                    printf("%d ", i);
                }
            }
            direction = -1;
        }
    }
    if (LOG_ON) {
        printf("\n");
        for (int i = 0; i < 32; i++) {
            printf("%.2f\t", c[i]);
        }
        printf("\n");
    }
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
int feedData(float *cur, int curStart, float *vol, int volStart, int date, char *extraMsg) {
    //step: 基本运算
    gTimer++;
    if (cur == NULL || vol == NULL) {
        return -1;
    }

    DateStruct ds;
    getDateByTimestamp(date, &ds);
    float effI = getEffectiveValue(cur, curStart, 128); //当前有效电流
    float effU = getEffectiveValue(vol, volStart, 128); //当前有效电压
    float activePower = getActivePower(cur, curStart, vol, volStart, 128); // 当前有功功率
    float reactivePower = getReactivePower(activePower, effI * effU);

    //更新能耗
    int timeDelta = date - gLastPowercostUpdateTime;
    if (timeDelta > 0 && timeDelta <= 30) { //限定为(0,30s],防止时间错乱带来的重大影响
        gPowerCost += gLastActivePower / 1000 * timeDelta;
        gLastPowercostUpdateTime = date;
    } else if (timeDelta > 30) {
        gLastPowercostUpdateTime = date;
    }

    if (gIsLibExpired) {
        //TODO:
        //对于过期库,添加随机干扰
    }
    //将新波形插入buff
    insertFloatArrayToBuff(gIWaveBuff, WAVE_BUFF_NUM, cur, 128);
    insertFloatArrayToBuff(gUWaveBuff, WAVE_BUFF_NUM, vol, 128);
    insertFloatToBuff(gActivePBuff, EFF_BUFF_NUM, activePower);

    //将功率插入10s短buff
    if (gTimer % 50 == 0) {
        PowerTrack powerTrack;
        powerTrack.activePower = activePower;
        powerTrack.reactivePower = reactivePower;
        powerTrack.sampleTime = date;
        powerTrack.totalPowerCost = 0;

        insertPackedDataToBuff((char*) gShortPowerTrack, sizeof(gShortPowerTrack), (char*) (&powerTrack),
                sizeof(powerTrack));
        if (gShortTrackNum < SHORT_TRACK_SIZE) {
            gShortTrackNum++;
        }
        if (gTimer % 500 == 0) {
            insertPackedDataToBuff((char*) gPowerTrack, sizeof(gPowerTrack), (char*) (&powerTrack),
                    sizeof(powerTrack));
            if (gPowerTrackNum < POWER_TRACK_SIZE) {
                gPowerTrackNum++;
            }
        }
    }

    //step: 事件监测
    int isStable = isPowerStable(gActivePBuff, EFF_BUFF_NUM, 15, 1); //稳态判断
    //负荷投切事件
    int switchEventHappen = 0;
    if (isStable > gLastStable) {
        //投切事件发生
        if (fabs(
                getStandardPower(activePower, effU)
                        - getStandardPower(gLastStableActivePower, gLastStableUEff)) > gMinEventDeltaPower) {
            switchEventHappen = 1;
        }
    }
    if (isStable != gLastStable) {
        if (LOG_ON) {
            printf("timer=%d stable=%d ap=%.2f rp=%.2f\n", gTimer, isStable, activePower,
                    getReactivePower(activePower, effI * effU));
        }
    }
    //负荷有功/无功缓慢上升事件
    int apRiseCounter = 0, rpRiseCounter = 0;
    if (gShortTrackNum >= SHORT_TRACK_SIZE) {
        for (int i = 0; i < SHORT_TRACK_SIZE - 1; i++) {
            if (gShortPowerTrack[i].activePower < gShortPowerTrack[i + 1].activePower) {
                apRiseCounter++;
            } else {
                apRiseCounter = 0;
            }
            if (gShortPowerTrack[i].reactivePower < gShortPowerTrack[i + 1].reactivePower) {
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
        zeroCrossLast = getZeroCrossIndex(gLastStableUWaveBuff, 0, 128);
        zeroCrossThis = getZeroCrossIndex(gUWaveBuff, 0, 128);

        for (int i = 0; i < 128; i++) {
            gDeltaIWaveBuff[i] = gIWaveBuff[zeroCrossThis + i] - gLastStableIWaveBuff[zeroCrossLast + i];
        }

        // feature1: fft
        getOddFft(gDeltaIWaveBuff, 0, deltaOddFft);
        // feature2:
        deltaEffI = getEffectiveValue(gDeltaIWaveBuff, 0, 128);
        deltaEffU = getEffectiveValue(gUWaveBuff, zeroCrossThis, 128);
        deltaActivePower = getActivePower(gDeltaIWaveBuff, 0, gUWaveBuff, zeroCrossThis, 128);
        deltaReactivePower = getReactivePower(deltaActivePower, deltaEffI * deltaEffU);

        // feature3:
        startupTime = gTransitTime;

        // feature4:
        iPulse = (gTransitTmpIMax - gLastStableIEff) / (effI - gLastStableIEff);

        gLastProcessedStableActivePower = activePower; //稳态功率缓慢上升的场景,processed不会变

        getCurrentWaveFeature(gDeltaIWaveBuff, 0, gUWaveBuff, zeroCrossThis, deltaEffI, deltaEffU, &deltaWf);

        if (LOG_ON) {
            printf("ext=%d flat=%d iPulse=%.2f st=%d dap=%.2f drp=%.2f fft=[%.2f %.2f %.2f]\n",
                    deltaWf.extremeNum, deltaWf.flatNum, iPulse, startupTime, deltaActivePower,
                    deltaReactivePower, deltaOddFft[0], deltaOddFft[1], deltaOddFft[2]);
        }
    }

    //step:业务处理
    //斩波式宿舍调压器检测
    //连续5秒有功和无功都增长
    int dormConverterAlarmTimeDelta = date - gDormConverterLastAlarmTime;
    if (apRiseCounter >= 5 && rpRiseCounter >= 5 && dormConverterAlarmTimeDelta >= 10) {
        int zeroCross = getZeroCrossIndex(gUWaveBuff, 0, 256);
        WaveFeature cwf;
        getCurrentWaveFeature(gIWaveBuff, zeroCross, gUWaveBuff, zeroCross, effI, effU, &cwf);
        gDormConverterAlarm = dormConverterAdjustingCheck(activePower, reactivePower, &cwf, NULL);
        if (gDormConverterAlarm) {
            if (LOG_ON) {
                printf("gDormConverterAlarm adjusting detected\n");
            }
            gDormConverterLastAlarmTime = date;
        }
    }

    if (switchEventHappen && !gDormConverterAlarm && dormConverterAlarmTimeDelta >= 10) {
        gDormConverterAlarm = dormConverterDetect(deltaActivePower, deltaReactivePower, &deltaWf, NULL);
        if (gDormConverterAlarm) {
            if (LOG_ON) {
                printf("gDormConverterAlarm detected flat=%d extre=%d md=%.2f mv=%.2f\n", deltaWf.flatNum,
                        deltaWf.extremeNum, deltaWf.maxDelta, deltaWf.maxValue);
            }
            gDormConverterLastAlarmTime = date;
        }
    }

    //充电检测
    if (switchEventHappen) {
        gChargingAlarm = chargingDetect(deltaOddFft, iPulse, deltaActivePower, deltaReactivePower, &deltaWf,
        NULL);
        if (gChargingAlarm) {
            if (LOG_ON) {
                printf("gChargingAlarm detected flat=%d extre=%d md=%.2f mv=%.2f\n", deltaWf.flatNum,
                        deltaWf.extremeNum, deltaWf.maxDelta, deltaWf.maxValue);
            }
        }
    }

    //恶性负载识别
    if (switchEventHappen) {

        char msg[50] = { 0 };
        gMaliLoadAlarm = maliciousLoadDetect(deltaOddFft, iPulse, deltaActivePower, deltaReactivePower, effU,
                activePower, reactivePower, &ds, msg);
        if (strlen(msg) > 0)
            printf("msg:%s\n", msg);
        if (gMaliLoadAlarm) {
            if (LOG_ON) {
                printf("gMaliLoadAlarm detected da=%.2f dr=%.2f eu=%.2f\n", deltaActivePower,
                        deltaReactivePower, effU);
            }
        }
    }
    //nilm

    //step:状态和变量更新
    // 稳态窗口的变量赋值和状态刷新
    if (isStable > 0) {
        for (int i = 0; i < WAVE_BUFF_NUM; i++) {
            gLastStableIWaveBuff[i] = gIWaveBuff[i];
            gLastStableUWaveBuff[i] = gUWaveBuff[i];
        }
        gLastStableIEff = effI;
        gLastStableUEff = effU;
        gLastStableActivePower = activePower;
        gTransitTime = 0;
        gTransitTmpIMax = 0;
    } else { // 非稳态
        gTransitTime++;
        if (effI > gTransitTmpIMax) {
            gTransitTmpIMax = effI;
        }
    }

    gLastStable = isStable;
    gLastActivePower = activePower;
    return 0;
}

extern char *APP_BUILD_DATE;

int initTpsonAlgoLib() {

    gIsLibExpired = isExpired(APP_BUILD_DATE, __DATE__, EXPIRED_DATE);
    if (gIsLibExpired) {
        return 1;
    }
    memset(gActivePBuff, 0, sizeof(gActivePBuff));
    memset(gIWaveBuff, 0, sizeof(gActivePBuff));
    memset(gUWaveBuff, 0, sizeof(gUWaveBuff));
    memset(gLastStableIWaveBuff, 0, sizeof(gLastStableIWaveBuff));
    memset(gLastStableUWaveBuff, 0, sizeof(gLastStableUWaveBuff));
    gLastStableIEff = 0;
    gLastStableUEff = 0;
    memset(gDeltaIWaveBuff, 0, sizeof(gDeltaIWaveBuff));

    gLastStable = 1; // 上个周期的稳态情况
    gTransitTime = 0; // 负荷变化过渡/非稳态时间
    gTransitTmpIMax = 0;

    gLastStableActivePower = 0; //稳态窗口下最近的有功功率,对于缓慢变化的场景会跟随变化
    gLastProcessedStableActivePower = 0; //上次经过稳态事件处理的有功功率,对于缓慢变化的场景不会跟随变化

    gTimer = 0;
    gIsLibExpired = 1;

    gChargingAlarm = 0;
    gDormConverterAlarm = 0;
    gDormConverterLastAlarmTime = 0;
    gMaliLoadAlarm = 0;

    memset(gShortPowerTrack, 0, sizeof(gShortPowerTrack));
    gShortTrackNum = 0;
    memset(gPowerTrack, 0, sizeof(gPowerTrack));
    gPowerTrackNum = 0;

    gPowerCost = 0;
    gLastPowercostUpdateTime = 0;
    gLastActivePower = 0;

    return 0;
}

int getAlgoVersion(void) {
    return VERSION[0] << 24 | VERSION[1] << 16 | VERSION[2] << 8 | VERSION[3];
}
