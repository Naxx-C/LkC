#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "algo_base_struct.h"
#include "algo_set_build.h"
#include "math_utils.h"
#include "nilm_utils.h"
#include "nilm_algo.h"
#include "nilm_feature.h"
#include "nilm_appliance.h"
#include "nilm_onlinelist.h"

#define PI 3.14159265f
#define MIN_POSSIBILITY 80//最小接受概率
const static char VERSION[] = { 1, 0, 0, 0 };

#define EFF_BUFF_NUM (50 * 3) // effective current buffer NUM
static float gActivePBuff[EFF_BUFF_NUM];

#define WAVE_BUFF_NUM 512
static float gIBuff[WAVE_BUFF_NUM]; // FIFO buff, pos0是最老的数据
static float gUBuff[WAVE_BUFF_NUM];
static float gLastStableIBuff[WAVE_BUFF_NUM];
static float gLastStableUBuff[WAVE_BUFF_NUM];
static float gLastStableIEff = 0;
static float gLastStableUEff = 0;
static float gDeltaIBuff[WAVE_BUFF_NUM];

static int gTransitTime = 0; // 负荷变化过渡/非稳态时间
static int gStartupTime = 0; // 启动时间
static float gTransitTmpIMax = 0;
static float gOddFft[5]; // 13579次谐波
static int gUtcTime = 0;
static float gLastActivePower = 0; //上个周期线路有功功率

static float gLastStableActivePower = 0; //稳态窗口下最近的有功功率,对于缓慢变化的场景会跟随变化
static float gLastProcessedStableActivePower = 0; //上次经过稳态事件处理的有功功率,对于缓慢变化的场景不会跟随变化

#define NILMEVENT_MAX_NUM   50 // 最多储存的事件
static NilmEvent gNilmEvents[NILMEVENT_MAX_NUM]; //待处理的事件
#define FOOTPRINT_SIZE 120 // 30s采样一次，1个小时的数据
static NilmPowerShaft gPowerShaft[FOOTPRINT_SIZE]; //存储电能足迹
static int gNilmWorkEnv = ENV_SIMPLE_TEST;
static int gNilmMinEventStep = 100;

const static int MAX_APPLIANCE_NUM = 100;
NilmAppliance gNilmAppliances[100];
int gAppliancesWriteIndex = 0; //init 0
int gAppliancesCounter = 0; //init 0

int nilmInit() {
    return 0;
}

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
int nilmMain(int utcTime, float effI, float effU, float activePower, float reactivePower,
        float deltaActivePower, float deltaReactivePower, float deltaEffI, float deltaEffU,
        float totalPowerCost, float oddFft[], int gLastStable) {
    // global timer
    gUtcTime = utcTime;

    // 稳态
    //投切事件发生

    // feature1: fft
    float harmonicBaseRatio = (oddFft[1] + oddFft[2] + oddFft[3] + oddFft[4]) * 100 / (oddFft[0] + 0.00001); //谐波基波比

    // feature2:
    float deltaApparentPower = deltaEffI * deltaEffU;
    float deltaPowerFactor = fabs(deltaActivePower) / deltaApparentPower;

    // feature3:
    gStartupTime = gTransitTime - EFF_BUFF_NUM + 1;

    // feature4:
    float iPulse = (gTransitTmpIMax - gLastStableIEff) / (effI - gLastStableIEff);

    NilmFeature nilmFeature;
    for (int i = 0; i < 5; i++) {
        nilmFeature.oddFft[i] = gOddFft[i];
    }
    nilmFeature.powerFactor = deltaPowerFactor;
    nilmFeature.pulseI = iPulse;
    nilmFeature.activePower = deltaActivePower * 220 * 220 / deltaEffU / deltaEffU;

    NilmEvent nilmEvent;
    memset(&nilmEvent, 0, sizeof(nilmEvent));
    nilmEvent.eventTime = utcTime;
    nilmEvent.voltage = effU;
    nilmEvent.eventId = nilmEvent.eventTime; // set time as id
    nilmEvent.activePower = activePower;

    for (int i = 0; i < 5; i++) {
        nilmEvent.feature[i] = gOddFft[i];
    }
    nilmEvent.feature[5] = nilmFeature.powerFactor;
    nilmEvent.feature[6] = nilmFeature.pulseI;
    nilmEvent.feature[7] = nilmFeature.activePower;

    clearMatchedList(); //先清除特征匹配列表

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
                euDis = 3; //to avoid useless exp cal cost
            double possibility = 100 / pow(5, euDis);

            if (possibility > MIN_POSSIBILITY) {
                //加入进匹配成功的临时列表
                MatchedAppliance ma;
                ma.activePower = deltaActivePower;
                ma.id = appliance.id;
                ma.possiblity = possibility;
                ApplianceAdditionalInfo *appInfo = getApplianceAdditionalInfo(ma.id);
                if (appInfo == NULL || (appInfo->supportedEnv & gNilmWorkEnv) > 0) {
                    addToMatchedList(&ma);
                    // printf("id=%d %.2f %.2f\n", ma.id, ma.activePower, ma.possiblity);
                }
            }
        }
    }

    //TODO:特殊电器处理,临时方案
    char isHeatingApp = 0;
    if (nilmFeature.powerFactor >= 0.97f && nilmFeature.pulseI <= 1.03f
            && nilmFeature.oddFft[0] / nilmFeature.oddFft[1] >= 20) {
        isHeatingApp = 1;
    }
    if (reactivePower >= 100 && activePower >= 300 && !isHeatingApp && !isInMatchedList(APPID_CLEANER)
            && !isInMatchedList(APPID_MICROWAVE_OVEN)) {
        WaveFeature sf;
        memset(&sf, 0, sizeof(sf));
        MatchedAppliance ma;
        memset(&ma, 0, sizeof(ma));
        ma.activePower = deltaActivePower;
        ma.possiblity = 88.8; //虚拟固定一个概率f
        if (sf.flatNum >= 14 && sf.flatNum <= 18 && sf.extremeNum == 1 && fabs(deltaReactivePower) >= 300
                && fabs(deltaActivePower) >= 300 && !isInMatchedList(APPID_HAIRDRYER)) {
            ma.id = APPID_HAIRDRYER;
        }

        if (ma.id == 0 && !isInMatchedList(APPID_VARFREQ_AIRCONDITIONER)
                && !isInMatchedList(APPID_FIXFREQ_AIRCONDITIONER)) {
            ma.id = APPID_VARFREQ_AIRCONDITIONER;
        }
        if (ma.id != 0) {
            ApplianceAdditionalInfo *appInfo = getApplianceAdditionalInfo(ma.id);
            if (appInfo == NULL || (appInfo->supportedEnv & gNilmWorkEnv) > 0) {
                addToMatchedList(&ma);
            }
        }
    }

    /*if (fabs(deltaReactivePower) >= 100 && fabs(deltaActivePower) >= 200 && harmonicBaseRatio >= 10) {

     WaveFeature sf;
     memset(&sf, 0, sizeof(sf));
     nilmGetStableFeature(gIBuff, zero2, 128, gUBuff, zero2, 128, effI, effU, activePower, &sf);
     MatchedAppliance ma;
     memset(&ma, 0, sizeof(ma));
     ma.activePower = deltaActivePower;
     ma.possiblity = 88.8; //虚拟固定一个概率

     //微波炉
     if (sf.flatNum == 0 && sf.extremeNum == 6 && harmonicBaseRatio >= 30
     && fabs(deltaReactivePower) >= 300 && fabs(deltaActivePower) >= 900
     && !isInMatchedList(APPID_MICROWAVE_OVEN)) {
     ma.id = APPID_MICROWAVE_OVEN;
     }
     //半波电吹风
     else if (sf.flatNum >= 14 && sf.flatNum <= 18 && sf.extremeNum == 1
     && fabs(deltaReactivePower) >= 300 && fabs(deltaActivePower) >= 300
     && !isInMatchedList(APPID_HAIRDRYER)) {
     ma.id = APPID_HAIRDRYER;
     }
     //定频空调(启动冲击大)
     else if ((iPulse >= 2.5 || fabs(deltaReactivePower) >= 300 || fabs(sf.reactivePower) >= 300)
     && !isInMatchedList(APPID_FIXFREQ_AIRCONDITIONER)
     && !isOnline(APPID_VARFREQ_AIRCONDITIONER)) { //变频定频互斥
     ma.id = APPID_FIXFREQ_AIRCONDITIONER;
     //TODO: tmp
     modifyLowPowerId(ma.id, 350);
     }
     //变频空调
     else if (!isInMatchedList(APPID_VARFREQ_AIRCONDITIONER)
     && !isOnline(APPID_FIXFREQ_AIRCONDITIONER)) {
     ma.id = APPID_VARFREQ_AIRCONDITIONER;
     //TODO: tmp
     modifyLowPowerId(ma.id, 350);
     }

     if (ma.id != 0) {
     ApplianceAdditionalInfo *appInfo = getApplianceAdditionalInfo(ma.id);
     if (appInfo == NULL || (appInfo->supportedEnv & gNilmWorkEnv) > 0) {
     addToMatchedList(&ma);
     //                    printf("id=%d %.2f %.2f\n", ma.id, ma.activePower, ma.possiblity);
     }
     }
     }*/

    signed char bestMatchedId = -1;
    float possibility = 0;
    getBestMatchedApp(deltaActivePower, &bestMatchedId, &possibility);
    updateOnlineListPowerByVol(effU); //矫正功率

    if (bestMatchedId > 0) {
        OnlineAppliance oa;
        memset(&oa, 0, sizeof(oa));
        oa.id = bestMatchedId;
        oa.activePower = deltaActivePower;
        oa.possiblity = possibility;
        oa.poweronTime = utcTime;
        oa.eventId = utcTime;
        oa.voltage = effU;
        oa.estimatedActivePower = deltaActivePower;
        if (nilmFeature.powerFactor >= 0.97f && nilmFeature.pulseI <= 1.03f
                && nilmFeature.oddFft[0] / nilmFeature.oddFft[1] >= 20) {
            oa.category = CATEGORY_HEATING;
        } else {
            oa.category = CATEGORY_UNBELIEVABLE;
        }
        setInitWaitingCheckStatus(&oa);

        updateOnlineListByEvent(&oa);

        //事件延时确认机制
        if (oa.isConfirmed != 1) {
            MatchedAppliance *matchedList = NULL;
            int matchedListCounter = 0;
            getMatchedList(&matchedList, &matchedListCounter);
            if (matchedList != NULL && matchedListCounter > 0) {
                for (int i = 0; i < matchedListCounter; i++) {
                    MatchedAppliance *m = &(matchedList[i]);
                    nilmEvent.possibleIds[i] = m->id;
                    //TODO:根据不同电器定义时间
                    //  ApplianceAdditionalInfo *info = getApplianceAdditionalInfo(m->id);
                    //  if (info != NULL) {
                    //      nilmEvent.countdownTimer[i] = info->minUseTime + utcTime;
                    //  }
                    if (oa.category == CATEGORY_HEATING) {
                        nilmEvent.delayedTimer[i] = 10 * 60;
                    } else {
                        nilmEvent.delayedTimer[i] = 60;
                    }
                }
            }
        }
        nilmEvent.possiblity = possibility;
        nilmEvent.applianceId = bestMatchedId;
    }

    //事件足迹更新
    NilmPowerShaft footprint;
//            footprint.deltaPower = deltaActivePower;
//            footprint.deltaPowerFactor = deltaPowerFactor;
//            footprint.eventTime = utcTime;
//            footprint.linePower = activePower;
//            footprint.voltage = effVoltage;
//            insertFifoCommon((char*) gFootprints, sizeof(gFootprints), (char*) (&footprint),
//                    sizeof(footprint));
    gLastProcessedStableActivePower = activePower;

    //TODO:空调临时方案，关空调有问题
    adjustAirConditionerPower(activePower);
    powerCheck(activePower);

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

    float cosineSimilarity = (float) (acos(getCosineSimilarity(fv1, normalizedFv2, 0, 5)) * 180 / PI);
    if (cosineSimilarity >= 12) {
        return NOT_MATCH_DIS;
    }

    gReorgFv1[0] = normalizePowerFactor(fv1[5]);
    gReorgFv1[1] = fv1[5] > 0 ? normalizePulseI(fv1[6]) : 0; // 功率负，电器关闭，没有冲击电流
    gReorgFv1[2] = normalizeActivePower(fv1[7]);

    gReorgFv2[0] = normalizedFv2[5];
    gReorgFv2[1] = fv1[5] > 0 ? normalizedFv2[6] : 0;
    gReorgFv2[2] = normalizedFv2[7];

    double euDis = getEuclideanDis(gReorgFv1, gReorgFv2, 3);

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
 * TODO:待完善
 * startTime: 分析的起始时间
 *
 */
void footprintsAnalyze(const NilmPowerShaft *const footprints, int footprintSize, int startTime,
        float minDeltaPower, FootprintResult *footprintResult) {

//    int flipTimes = 0; //切换次数
//    int maybeSteplessChange = 0; //判断是否有疑似无级变化
//    int timestartIndex = 0;
//    for (int i = 0; i < footprintSize; i++) {
//        NilmPowerShaft fp = footprints[i];
//        if (fp.eventTime < startTime) {
//            timestartIndex = i + 1; //记录起始处理的index
//            continue;
//        }
//
//        if (fp.deltaPower > minDeltaPower) {
//            flipTimes++;
//        } else {
//            continue;
//        }
//
//        if (fp.deltaPowerFactor <= 0.985) {
//            footprintResult->haveNonPureResLoad = 1;
//        }
//
//        if (i < footprintSize - 1 && maybeSteplessChange == 0) {
//            float delta = footprints[i + 1].deltaPower;
//
//            if (delta > 0 && i < footprintSize - 1) {
//                float lineDelta = footprints[i + 1].linePower - footprints[i].linePower;
//                //delta变化量比线路总负荷变化明显小 TODO:not best
//                if (lineDelta - delta > 120 && footprints[i + 1].eventTime - footprints[i].eventTime < 180) {
//                    maybeSteplessChange = 1;
//                }
//            }
//        }
//    }
//
//    footprintResult->flipTimes = flipTimes;
//    footprintResult->haveSteplessChange = 1;
}

void handleApplianceOff(OnlineAppliance *oa) {
    if (oa->isConfirmed == 0) {

        int runningTime = (gUtcTime - oa->poweronTime) / 60; //min
        for (int i = 0; i < NILMEVENT_MAX_NUM; i++) {
            NilmEvent *ne = &(gNilmEvents[i]);

            if (ne->eventId == oa->eventId) {
                //先确定best id
                ApplianceAdditionalInfo *appInfo = getApplianceAdditionalInfo(ne->applianceId);
                if (appInfo == NULL //为空默认为无限制,判定为pass
                        || (appInfo != NULL && appInfo->maxUseTime != 0 && runningTime >= appInfo->minUseTime
                                && runningTime <= appInfo->maxUseTime)) {
                    oa->isConfirmed = 1;
                    break;
                }

                //best id未能确定，check候选id
                for (int j = 0; j < MATCHEDLIST_MAX_NUM; j++) {
                    if (ne->possibleIds[j] > 0) {
                        appInfo = getApplianceAdditionalInfo(ne->possibleIds[j]);
                        //此处简化，只选第1个
                        if (ne->possibleIds[j] != ne->applianceId) {
                            if (appInfo == NULL
                                    || (appInfo != NULL && appInfo->maxUseTime != 0
                                            && runningTime >= appInfo->minUseTime
                                            && runningTime <= appInfo->maxUseTime)) {
                                oa->id = ne->possibleIds[j];
                                oa->isConfirmed = 1;
                                break;
                            }
                        }
                    }
                }

                break;
            }
        }
    }

    if (oa->isConfirmed == 0 && oa->category != CATEGORY_HEATING) {
        oa->category = CATEGORY_UNKNOWN;
    }

//TODO:发送功耗数据
    printf("id=%d powercost=%.2f\n", oa->id, oa->powerCost);

}

//TODO:need a smarter way later
void setInitWaitingCheckStatus(OnlineAppliance *oa) {

    oa->isConfirmed = 0;
    if (oa->id == APPID_MICROWAVE_OVEN || oa->id == APPID_CLEANER) {
        oa->isConfirmed = 1;
    } else if (oa->id == APPID_WATER_HEATER && oa->activePower >= 2500) {
        oa->isConfirmed = 1;
    }
}

void checkWaitingNilmEvents(int currentTime) {
    static int lastCheckTime = 0;
    int timeGap = currentTime - lastCheckTime;
    if (timeGap != 0) { //每n秒检查一次 && timeGap % n == 0

        //延时确认事件处理
        OnlineAppliance *onlineList = NULL;
        int onlineListCounter = 0;
        getOnlineList(&onlineList, &onlineListCounter);
        if (onlineList != NULL && onlineListCounter > 0) {
            for (int i = 0; i < NILMEVENT_MAX_NUM; i++) {
                NilmEvent *ne = &(gNilmEvents[i]);
                if (ne->eventId > 0) {
                    if (isOnlineByEventId(ne->eventId)) { //如不在线,复位event
                        ne->eventId = 0;
                        continue;
                    }

                    char eventWaitingCheck = 0;
                    for (int j = 0; j < MATCHEDLIST_MAX_NUM; j++) {
                        if (ne->delayedTimer[j] >= currentTime) {
                            if (currentTime >= ne->delayedTimer[j]) {
                                FootprintResult fr;
                                footprintsAnalyze(gPowerShaft, FOOTPRINT_SIZE, ne->eventTime, 200, &fr);
                            }
                            ne->delayedTimer[j] = 0; //delayedTimer复位
                        } else {
                            eventWaitingCheck = 1;
                        }
                    }
                    if (eventWaitingCheck == 0) {
                        ne->eventId = 0; //删除待处理event
                    }
                }
            }
        }
    }
    lastCheckTime = currentTime;
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
