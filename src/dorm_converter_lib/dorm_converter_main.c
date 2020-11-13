#include "file_utils.h"
#include "string_utils.h"
#include "time_utils.h"
#include <stdio.h>
#include <sys/stat.h>
#include "dirent.h"
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include "fft.h"

#include "dorm_converter_algo.h"

#define EFF_BUFF_NUM (50 * 1) // effective current buffer NUM
static float gActivePBuff[EFF_BUFF_NUM];

#define WAVE_BUFF_NUM 512
static float gIBuff[WAVE_BUFF_NUM]; // FIFO buff, pos0是最老的数据
static float gUBuff[WAVE_BUFF_NUM];
static float gLastStableIBuff[WAVE_BUFF_NUM];
static float gLastStableUBuff[WAVE_BUFF_NUM];
static float gLastStableIEff = 0;
static float gLastStableUEff = 0;
static float gDeltaIBuff[WAVE_BUFF_NUM];

static int gLastStable = 1; // 上个周期的稳态情况

static int gTransitTime = 0; // 负荷变化过渡/非稳态时间
static int gStartupTime = 0; // 启动时间
static float gTransitTmpIMax = 0;
static float gOddFft[5]; // 13579次谐波

static float gLastStableActivePower = 0; //稳态窗口下最近的有功功率,对于缓慢变化的场景会跟随变化
static float gLastProcessedStableActivePower = 0; //上次经过稳态事件处理的有功功率,对于缓慢变化的场景不会跟随变化

// 过零检测
static int zeroCrossPoint(float sample[], int start, int end) {
    for (int i = start; i < end; i++) {
        if (sample[i] <= 0.0f && sample[i + 1] > 0.0f) {
            if (i - 1 >= start && i + 2 < end) {
                if (sample[i - 1] < 0.0f && sample[i + 2] > 0.0f)
                    return i + 1;
            } else {
                return i + 1;
            }
        }
    }
    return -1;
}

// 按照顺序插入，最新插入的数据排在buff后面
static void insertFifoBuff(float buff[], int buffSize, float newData[], int dataLen) {

    int end = buffSize - dataLen;
    for (int i = 0; i < end; i++) {
        buff[i] = buff[i + dataLen];
    }

    for (int i = 0; i < dataLen; i++) {
        buff[buffSize - dataLen + i] = newData[i];
    }
}

// 只插入一条，最新插入的数据排在buff后面
static void insertFifoBuffOne(float buff[], int buffSize, float newData) {

    int end = buffSize - 1;
    for (int i = 0; i < end; i++) {
        buff[i] = buff[i + 1];
    }
    buff[buffSize - 1] = newData;
}
/**
 * 有功功率，可正可负
 */
static float getActivePower(float current[], int indexI, float voltage[], int indexU, int len) {

    float activePower = 0;
    for (int i = 0; i < len; i++) {
        activePower += current[indexI + i] * voltage[indexU + i];
    }
    activePower /= len;
    return activePower; // > 0 ? activePower : -activePower;
}

/**
 * 有效值
 */
static float getEffectiveValue(float inputs[], int start, int len) {
    float sumOfSquares = 0;
    int end = start + len;
    for (int i = start; i < end; i++)
        sumOfSquares += inputs[i] * inputs[i];

    return (float) sqrt(sumOfSquares / len);
}

static int isPowerStable(float inputs[], int len, float absThresh, int relativeRatio) {

    float average = 0, absAverage = 0;
    for (int i = 0; i < len; i++) {
        average += inputs[i];
    }
    average /= len;
    absAverage = average >= 0 ? average : -average;
    for (int i = 0; i < len; i++) {
        float absDelta = inputs[i] - average;
        absDelta = absDelta >= 0 ? absDelta : -absDelta;

        if (absDelta > absThresh && absDelta * 100 > absAverage * relativeRatio) {
            return 0;
        }
    }

    return 1;
}

#define FOOTPRINT_SIZE 10 // 1s采样一次，1个小时的数据
static PowerShaft gPowerShaft[FOOTPRINT_SIZE]; //存储电能足迹
static int gPowerShaftSize = 0;

static int gTimer = 0;
// 20ms调用一次
static int stableAnalyze(float current[], float voltage[], int length, float effCurrent, float effVoltage,
        float realPower, float oddFft[]) {
    gTimer++;
    if (length > WAVE_BUFF_NUM || WAVE_BUFF_NUM % length != 0) {
        return -2;
    }

    insertFifoBuff(gIBuff, WAVE_BUFF_NUM, current, length);
    insertFifoBuff(gUBuff, WAVE_BUFF_NUM, voltage, length);

    // 当前有功功率
    float activePower = realPower >= 0 ? realPower : getActivePower(current, 0, voltage, 0, length);
    // if no valid effCurrent pass in, we calculate it.
    float effI = effCurrent >= 0 ? effCurrent : getEffectiveValue(current, 0, length);
    float effU = effVoltage >= 0 ? effVoltage : getEffectiveValue(voltage, 0, length);

    float reactivePower = sqrtf(effI * effI * effU * effU - activePower * activePower);

    insertFifoBuffOne(gActivePBuff, EFF_BUFF_NUM, activePower);
    int isStable = isPowerStable(gActivePBuff, EFF_BUFF_NUM, 15, 1);

    if (isStable != gLastStable) {
        printf(">>>gTimer=%d stable status change, stable=%d activePower=%.2f\n", gTimer, isStable,
                activePower);
    }

    //无功缓慢上升检测
    if (gTimer % 50 == 0) {
        PowerShaft powerShaft;
        powerShaft.activePower = activePower;
        powerShaft.reactivePower = reactivePower;
        insertFifoCommon((char*) gPowerShaft, sizeof(gPowerShaft), (char*) (&powerShaft), sizeof(powerShaft));
        if (gPowerShaftSize < FOOTPRINT_SIZE) {
            gPowerShaftSize++;
        }
        // 已存满
        int riseCounter = -1;
        if (gPowerShaftSize >= FOOTPRINT_SIZE) {
            for (int i = 0; i < FOOTPRINT_SIZE - 1; i++) {
                if (gPowerShaft[i].reactivePower < gPowerShaft[i + 1].reactivePower
                        && gPowerShaft[i].activePower < gPowerShaft[i + 1].activePower) {
                    riseCounter++;
                } else {
                    riseCounter = 0;
                }
            }
        }
        if (riseCounter >= FOOTPRINT_SIZE / 2) {
            int zero2 = zeroCrossPoint(gUBuff, 0, 128);
            DormWaveFeature cwf;
            getDormWaveFeature(gIBuff, zero2, gUBuff, zero2, effI, effU, activePower, &cwf);

            if (cwf.flatNum >= 14 && cwf.maxDelta / (cwf.maxValue + 0.0001f) >= 0.8f && activePower >= 150
                    && reactivePower >= 150 && cwf.extremeNum >= 2 && cwf.extremeNum <= 3) {
                printf("dorm slowly changed\n");
            }
        }
    }

    // 稳态差分检测
    if (isStable > 0) {

        //投切事件发生
        if (gLastStable == 0
                && fabs(
                        activePower * 220 * 220 / effU / effU
                                - gLastStableActivePower * 220 * 220 / gLastStableUEff / gLastStableUEff)
                        > 85) {

            // 计算差分电流
            int zero1 = zeroCrossPoint(gLastStableUBuff, 0, 128);
            int zero2 = zeroCrossPoint(gUBuff, 0, 128);

            for (int i = 0; i < 128; i++) {
                gDeltaIBuff[i] = gIBuff[zero2 + i] - gLastStableIBuff[zero1 + i];
            }

            // feature1: fft
            for (int i = 0; i < 5; i++) {
                gOddFft[i] = oddFft[i];
            }
            // feature2:
            float deltaActivePower = getActivePower(gDeltaIBuff, 0, gUBuff, zero2, 128);
            float deltaEffI = getEffectiveValue(gDeltaIBuff, 0, 128);
            float deltaEffU = getEffectiveValue(gUBuff, zero2, 128);

            // feature3:
            gStartupTime = gTransitTime - EFF_BUFF_NUM + 1;

            // feature4:
            float iPulse = (gTransitTmpIMax - gLastStableIEff) / (effI - gLastStableIEff);

            char errMsg[50];
            memset(errMsg, 0, sizeof(errMsg));

            int iscd = isDormConverter(gDeltaIBuff, 0, gUBuff, zero2, deltaEffI, deltaEffU,
                    deltaActivePower, errMsg);
            printf("%d errMsg:%s\n", iscd, errMsg != NULL ? errMsg : "ok");
            gLastProcessedStableActivePower = activePower;
        } else { //非投切事件处理
        }

        // 稳态窗口的变量赋值和状态刷新
        for (int i = 0; i < WAVE_BUFF_NUM; i++) {
            gLastStableIBuff[i] = gIBuff[i];
            gLastStableUBuff[i] = gUBuff[i];
        }
        gLastStableIEff = effI;
        gLastStableUEff = effU;
        gLastStableActivePower = activePower;
        gTransitTime = 0;
        gTransitTmpIMax = 0;
    } else { // 非稳态
        gTransitTime++;
//        printf("no=\t%.2f\n", activePower);

        if (effI > gTransitTmpIMax) {
            gTransitTmpIMax = effI;
//            printf("gTransitTmpIMax=%.2f\n", gTransitTmpIMax);
        }
    }
    gLastStable = isStable;

    return 0;
}

static char *dirPath = "F:\\Tmp\\tiaoya";
//static char *dirPath = "F:\\Tmp\\nilm";
int dorm_converter_main() {

    int ret = dormConverterAlgoInit();
    setDormConverterMode(DORM_CONVERTER_SENSITIVITY_MEDIUM);
    if (ret != 0) {
        printf("some error\n");
        return -1;
    }
    DIR *dir = NULL;
    struct dirent *entry;

    int pointNum = 0, fileIndex = 0;
    if ((dir = opendir(dirPath)) == NULL) {
        printf("opendir failed\n");
    } else {
        char csv[128];
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
                    || !endWith(entry->d_name, ".csv")) //|| !startWith(entry->d_name, "0")) ///current dir OR parrent dir
                continue;
            int convertHardcode = 1; //TODO:注意单相电抓的数据是反的
            if (startWith(entry->d_name, "elec_"))
                convertHardcode = 1;

            fileIndex = 0;
            memset(csv, 0, sizeof(csv));
            snprintf(csv, sizeof(csv) - 1, "%s\\%s", dirPath, entry->d_name);
            printf("filepath=%s filetype=%d\n", csv, entry->d_type); //输出文件或者目录的名称
            /**parse csv*/
            float current[128], voltage[128];
            FILE *f = fopen(csv, "rb");
            printf("%s\n", csv);
//            FILE *f = fopen(csvPath, "rb");
            int i = 0;
            while (!feof(f) && !ferror(f)) {
                float cur, vol;
                fscanf(f, "%f,%f", &cur, &vol);
                current[i] = cur * convertHardcode;
                voltage[i] = vol;
                fileIndex++;
                i++;
                if (i >= 128) {
                    float fft[128], oddFft[5];
                    do_fft(current, fft);

                    for (int j = 0; j < 5; j++) {
                        oddFft[j] = fft[j * 2 + 1];
                    }
                    stableAnalyze(current, voltage, 128, -1, -1, -1, oddFft);
                    i = 0;
                }
                pointNum++;
            }
            fclose(f);
        }

    }
    if (dir != NULL)
        closedir(dir);

    return 0;
}
