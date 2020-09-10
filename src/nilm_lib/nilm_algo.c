#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "nilm_algo.h"
#include "nilm_utils.h"
#include "nilm_feature.h"
#include "nilm_appliance.h"
#include "nilm_onlinelist.h"

#define PI 3.14159265f
#define HOUR 3600
#define DAY (3600*24)
const static char VERSION[] = { 1, 0, 0, 0 };

#define EFF_BUFF_NUM (50 * 3) // effective current buffer NUM
static float gActivePBuff[EFF_BUFF_NUM];

#define BUFF_NUM 512
static float gIBuff[BUFF_NUM]; // FIFO buff, pos0是最老的数据
static float gUBuff[BUFF_NUM];
static float gLastStableIBuff[BUFF_NUM];
static float gLastStableUBuff[BUFF_NUM];
static float gLastStableIEff = 0;
static float gLastStableUEff = 0;
static float gDeltaIBuff[BUFF_NUM];

static int gLastStable = 1; // 上个周期的稳态情况

static int gTransitTime = 0; // 负荷变化过渡/非稳态时间
static int gStartupTime = 0; // 启动时间
static float gIPulse = 0; // 脉冲电流比
static float gTransitTmpIMax = 0;
static float gOddFft[5]; // 13579次谐波
static int gTimer = 0;
static float gTotalPowerCost = 0; //kws

static float gLastStableActivePower = 0;

#define NILMEVENT_MAX_NUM   100 // 最多储存的事件
static NilmEvent gNilmEvents[NILMEVENT_MAX_NUM];
#define FOOTPRINT_SIZE 100
static NilmEventFootprint gFootprints[FOOTPRINT_SIZE]; //存储事件足迹
static int gNilmWorkEnv = ENV_SIMPLE_TEST;
static int gNilmMinEventStep = 70;

const static int MAX_APPLIANCE_NUM = 100;
NilmAppliance gNilmAppliances[100];
int gAppliancesWriteIndex = 0;    //init 0
int gAppliancesCounter = 0;    //init 0

NilmAppliance* createNilmAppliance(char name[], int nameLen, char id, float accumulatedPower) {

    if (gAppliancesWriteIndex >= MAX_APPLIANCE_NUM)
        gAppliancesWriteIndex = 0;

    NilmAppliance *na = &(gNilmAppliances[gAppliancesWriteIndex]);

    memset(na->name, 0, sizeof(na->name));
    memcpy(na->name, name, nameLen);
    na->id = id;
    na->accumulatedPower = accumulatedPower;
    for (int i = 0; i < APP_FEATURE_NUM; i++) {
        na->featureWriteIndex = 0;
        na->featureWriteIndex = 0;
    }

    gAppliancesWriteIndex++;
    if (gAppliancesCounter < MAX_APPLIANCE_NUM)
        gAppliancesCounter++;
    return na;
}

// 20ms调用一次
int nilmAnalyze(float current[], float voltage[], int length, int utcTime, float effCurrent, float effVoltage,
        float realPower, float oddFft[]) {
    // global timer
    gTimer++;

    if (length > BUFF_NUM || BUFF_NUM % length != 0) {
        return -2;
    }

    insertFifoBuff(gIBuff, BUFF_NUM, current, length);
    insertFifoBuff(gUBuff, BUFF_NUM, voltage, length);

    //首先更新功耗数据
    updatePowercost(utcTime, &gTotalPowerCost, gActivePBuff[EFF_BUFF_NUM - 1]);
    // 当前有功功率
    float activePower = realPower >= 0 ? realPower : nilmActivePower(current, 0, voltage, 0, length);
    // if no valid effCurrent pass in, we calculate it.
    float effI = effCurrent >= 0 ? effCurrent : nilmEffectiveValue(current, 0, length);
    float effU = effVoltage >= 0 ? effVoltage : nilmEffectiveValue(voltage, 0, length);

    insertFifoBuffOne(gActivePBuff, EFF_BUFF_NUM, activePower);
    int isStable = nilmIsStable(gActivePBuff, EFF_BUFF_NUM, 30, 2);

    if (isStable != gLastStable) {
        printf(">>>stable status change, stable=%d\n", isStable);
    }
    // 稳态
    if (isStable > 0) {

        if (gLastStable == 0
                && fabs(
                        activePower * 220 * 220 / effU / effU
                                - gLastStableActivePower * 220 * 220 / gLastStableUEff / gLastStableUEff)
                        > gNilmMinEventStep) {

            // 计算差分电流
            int zero1 = zeroCrossPoint(gLastStableUBuff, 0, 128);
            int zero2 = zeroCrossPoint(gUBuff, 0, 128);

            for (int i = 0; i < 128; i++) {
                gDeltaIBuff[i] = gIBuff[zero2 + i] - gLastStableIBuff[zero1 + i];
            }

            // TODO:是否要加筛选机制，待验证

            // feature1: fft
            for (int i = 0; i < 5; i++) {
                gOddFft[i] = oddFft[i];
            }

            // feature2:
            float deltaActivePower = nilmActivePower(gDeltaIBuff, 0, gUBuff, zero2, 128);
            float deltaEffI = nilmEffectiveValue(gDeltaIBuff, 0, 128);
            float deltaEffU = nilmEffectiveValue(gUBuff, zero2, 128);
            float deltaApparentPower = deltaEffI * deltaEffU;
            float deltaPowerFactor = deltaActivePower / deltaApparentPower;

            // feature3:
            gStartupTime = gTransitTime - EFF_BUFF_NUM + 1;

            // feature4:
            gIPulse = (gTransitTmpIMax - gLastStableIEff) / (effI - gLastStableIEff);

            NilmFeature nilmFeature;
            for (int i = 0; i < 5; i++) {
                nilmFeature.oddFft[i] = gOddFft[i];
            }
            nilmFeature.powerFactor = deltaPowerFactor;
            nilmFeature.pulseI = gIPulse;
            nilmFeature.activePower = deltaActivePower * 220 * 220 / deltaEffU / deltaEffU;

            NilmEvent nilmEvent;
            memset(&nilmEvent, 0, sizeof(nilmEvent));
            nilmEvent.eventTime = utcTime;
            nilmEvent.voltage = effU;
            nilmEvent.eventId = nilmEvent.eventTime;            // set time as id
            nilmEvent.activePower = activePower;
            nilmEvent.action = deltaActivePower > 0 ? ACTION_ON : ACTION_OFF;

            for (int i = 0; i < 5; i++) {
                nilmEvent.feature[i] = gOddFft[i];
            }
            nilmEvent.feature[5] = nilmFeature.powerFactor;
            nilmEvent.feature[6] = nilmFeature.pulseI;
            nilmEvent.feature[7] = nilmFeature.activePower;

            clearMatchedList();            //先清除特征匹配列表
            for (int i = 0; i < gAppliancesCounter; i++) {
                NilmAppliance appliance = gNilmAppliances[i];
                for (int j = 0; j < appliance.featureCounter; j++) {

                    NilmFeature normalizedFeature = appliance.nilmNormalizedFeatures[j];
                    float fv2[8];
                    for (int k = 0; k < 5; k++) {
                        fv2[k] = normalizedFeature.oddFft[k];
                    }
                    fv2[5] = normalizedFeature.powerFactor;
                    fv2[6] = normalizedFeature.pulseI;
                    fv2[7] = normalizedFeature.activePower;
                    double euDis = featureMatch(nilmEvent.feature, fv2);
                    if (euDis > 3)
                        euDis = 3;            //to avoid useless exp cal cost
                    double possibility = 100 / pow(5, euDis);

                    if (possibility > 70) {
                        MatchedAppliance ma;
                        ma.activePower = deltaActivePower;
                        ma.id = appliance.id;
                        ma.possiblity = possibility;
                        addToMatchedList(&ma);
                    }
                }
            }

            signed char bestMatchedId = -1;
            float possibility = 0;
            getBestMatchedApp(deltaActivePower, &bestMatchedId, &possibility);
            updateOnlineAppPower(effU);            //矫正功率

            if (bestMatchedId > 0) {
                OnlineAppliance oa;
                oa.id = bestMatchedId;
                oa.activePower = deltaActivePower;
                oa.possiblity = possibility;
                oa.poweronTime = utcTime;
                oa.voltage = effU;
                oa.estimatedActivePower = deltaActivePower;
                if (nilmFeature.powerFactor >= 0.97f && nilmFeature.pulseI <= 1.03f
                        && nilmFeature.oddFft[0] / nilmFeature.oddFft[1] >= 20) {
                    oa.category = CATEGORY_HEATING;
                } else {
                    oa.category = CATEGORY_UNBELIEVABLE;
                }
                updateOnlineList(&oa);
            }
            nilmEvent.possiblity = possibility;
            nilmEvent.applianceId = bestMatchedId;

            abnormalCheck(realPower);

            //事件足迹更新
            NilmEventFootprint footprint;
            footprint.deltaPower = deltaActivePower;
            footprint.deltaPowerFactor = deltaPowerFactor;
            footprint.eventTime = utcTime;
            footprint.linePower = activePower;
            footprint.voltage = effVoltage;
            insertFifoCommon((char*) gFootprints, sizeof(gFootprints), (char*) (&footprint),
                    sizeof(footprint));
        }

        // 稳态赋值或状态刷新
        for (int i = 0; i < BUFF_NUM; i++) {
            gLastStableIBuff[i] = gIBuff[i];
            gLastStableUBuff[i] = gUBuff[i];
        }
        gLastStableIEff = effI;
        gLastStableUEff = effU;
        gLastStableActivePower = activePower;
        gTransitTime = 0;
        gTransitTmpIMax = 0;

    } else {                // 非稳态
        gTransitTime++;
        if (effI > gTransitTmpIMax) {
            gTransitTmpIMax = effI;
        }
    }
    gLastStable = isStable;

    return 0;
}

static float gReorgFv1[3];
static float gReorgFv2[3];
static const double NOT_MATCH_DIS = 999;

// fv1为原始特征,归一化会损失一些必要信息;fv2为特征库里归一化后的向量,减少无用的重复计算
double featureMatch(float fv1[], float normalizedFv2[]) {

    // Step1: 先判断必要条件，一票否决
    if (fv1 == NULL || normalizedFv2 == NULL) {
        return NOT_MATCH_DIS;
    }
    // 基于功率快捷筛选,功率差300w或者功率比超过3倍,直接退出
    if (fabs(fv1[0] - normalizedFv2[0]) > 1.5f || fv1[0] / (normalizedFv2[0] + 0.01f) > 3.0f
            || normalizedFv2[0] / (fv1[0] + 0.01f) < 0.33f) {
        return NOT_MATCH_DIS;
    }
    // 基波分量占比
    if (fabs(getRatioLevel(fv1) - getRatioLevel(normalizedFv2)) >= 2) {
        return NOT_MATCH_DIS;
    }

    float cosineSimilarity = (float) (acos(cosine(fv1, normalizedFv2, 0, 5)) * 180 / PI);
    if (cosineSimilarity >= 12) {
        return NOT_MATCH_DIS;
    }

    gReorgFv1[0] = normalizePowerFactor(fv1[5]);
    gReorgFv1[1] = fv1[5] > 0 ? normalizePulseI(fv1[6]) : 0;                // 功率负，电器关闭，没有冲击电流
    gReorgFv1[2] = normalizeActivePower(fv1[7]);

    gReorgFv2[0] = normalizedFv2[5];
    gReorgFv2[1] = fv1[5] > 0 ? normalizedFv2[6] : 0;
    gReorgFv2[2] = normalizedFv2[7];

    double euDis = euclideanDis(gReorgFv1, gReorgFv2, 0, 3);

    euDis = sqrt(euDis * euDis + cosineSimilarity / 100 * cosineSimilarity / 100);

    return euDis;
}

int getRatioLevel(float fv[]) {
    float highOddFftSum = 0.00001f;
    for (int i = 1; i < 5; i++) {
        highOddFftSum += fv[i];
    }
    float r = fv[0] / highOddFftSum;
    if (r < 1)
        return 0;
    else if (r >= 1 && r < 5)
        return 1;
    else if (r >= 5 && r < 20)
        return 2;
    else if (r >= 20 && r < 40)
        return 3;
    else
        return 4;
}

/*
 * startTime: 分析的最小起始时间
 */
int footprintsAnalyze(const NilmEventFootprint *const footprints, int footprintSize, int startTime) {

    int flipTimes = 0;       //非对称切换次数
    int maybeSteplessChange = 0;  //判断是否有疑似无级变化
    int timestartIndex = 0;
    for (int i = 0; i < footprintSize; i++) {
        NilmEventFootprint footprint = footprints[i];
        if (footprint.eventTime < startTime) {
            timestartIndex = i;  //记录起始处理的index
            continue;
        }

        if (i < footprintSize - 1 && maybeSteplessChange == 0) {
            float delta = footprints[i + 1].deltaPower;

            if (delta > 0) {
                float lineDelta = footprints[i + 1].linePower - footprints[i].linePower;
                //delta变化量比线路总负荷变化明显小 TODO:not best
                if (lineDelta - delta > 300 && footprints[i + 1].eventTime - footprints[i].eventTime < 180) {
                    maybeSteplessChange = 1;
                }
            }
        }
    }

    return 1;
}

/*
 * cur/vol:电压过0点时的
 */
#define BATCH 4
void stableFeatureExtract(float *cur, int curStart, int curLen, float *vol, int volStart, int volLen) {

    // 窗口均值滤波
    if (curLen % BATCH != 0)
        return;
    float max = -999, min = 999;
    float c[33];  // 扩充1位
    for (int i = 0; i < 128; i += BATCH) {
        for (int j = 0; j < BATCH; j++) {
            c[i / BATCH] += cur[curStart + i + j] / BATCH;
        }
        if (c[i / BATCH] < min)
            min = c[i / BATCH];
        if (c[i / BATCH] > max)
            max = c[i / BATCH];
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
    float valueThresh = 0.25f, deltaThresh = ad / 3;
    for (int i = 0; i < 32; i++) {
        // 周边至少一个点也是平肩
        if ((i >= 2 && fabs(c[i]) < valueThresh && fabs(c[i - 1]) < valueThresh
                && fabs(c[i - 2]) < valueThresh && fabs(c[i] - c[i - 2]) < deltaThresh
                && fabs(c[i] - c[i - 1]) < deltaThresh && fabs(c[i - 1] - c[i - 2]) < deltaThresh)
                || (i <= 30 && fabs(c[i]) < valueThresh && fabs(c[i + 1]) < valueThresh
                        && fabs(c[i + 2]) < valueThresh && fabs(c[i + 2] - c[i]) < deltaThresh
                        && fabs(c[i + 1] - c[i]) < deltaThresh && fabs(c[i + 2] - c[i + 1]) < deltaThresh)) {
            flatNum++;
            flatBitmap = flatBitmap | (0x1 << i);
        }
    }
    // 极值点个数，极值点非平肩
    for (int i = 0; i < 32; i++) {
        if (c[i + 1] - c[i] > 0 && (flatBitmap & (0x1 << i)) == 0) {
            if (direction < 0) {  // && fabs(c[i]) > averageDelta / 5
                extremeNum++;
            }
            direction = 1;
        } else if (c[i + 1] - c[i] < 0 && (flatBitmap & (0x1 << i)) == 0) {
            if (direction > 0) {
                extremeNum++;
            }
            direction = -1;
        }
    }
    return;
}

int getNilmAlgoVersion() {
    return VERSION[0] << 24 | VERSION[1] << 16 | VERSION[2] << 8 | VERSION[3];
}

void setNilmWorkEnv(int env) {
    gNilmWorkEnv = env;
}

int getNilmWorkEnv() {
    return gNilmWorkEnv;
}

void setNilmMinEventStep(int minEventStep) {
    gNilmMinEventStep = minEventStep;
}

void nilm_algo_main() {

}
