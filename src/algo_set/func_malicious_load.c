#include "algo_base_struct.h"
#include "data_structure_utils.h"
#include "power_utils.h"
#include "time_utils.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define POWER_WHITELIST_SIZE 10
static float gPowerWhitelist[POWER_WHITELIST_SIZE] = { 0 };
static int gPowerWhitelistNum = 0;

static float gPowerMinRatio = 0.9f;
static float gPowerMaxRatio = 1.1f;

void setMaliLoadWhitelistMatchRatio(float minRatio, float maxRatio) {
    gPowerMinRatio = minRatio;
    gPowerMaxRatio = maxRatio;
}

static float gMinPower = 200;
void setMaliLoadMinPower(float minPower) {
    gMinPower = minPower;
}

//添加白名单功率，最多10个，超过10个自动替换最早的，重复添加的自动过滤
void addMaliciousLoadWhitelist(float power) {
    if (power <= 0)
        return;

    for (int i = 0; i < gPowerWhitelistNum; i++) {
        //已存在,返回
        if (fabs(power - gPowerWhitelist[i]) < 10) {
            return;
        }
    }

    insertFloatToBuff(gPowerWhitelist, POWER_WHITELIST_SIZE, power);
    if (gPowerWhitelistNum < POWER_WHITELIST_SIZE) {
        gPowerWhitelistNum++;
    }
}

int getMaliciousLoadWhitelist(float outWhilelist[10]) {
    int outIndex = 0;
    for (int i = 0; i < gPowerWhitelistNum && outIndex < 10; i++) {
        if (gPowerWhitelist[i] > LF) {
            outWhilelist[outIndex++] = gPowerWhitelist[i];
        }
    }
    return gPowerWhitelistNum;
}

static int isInMaliciousLoadWhitelist(float power) {
    if (power <= 0)
        return 0;
    for (int i = 0; i < POWER_WHITELIST_SIZE; i++) {
        float ratio = gPowerWhitelist[i] / power;
        if (ratio >= gPowerMinRatio && ratio <= gPowerMaxRatio) {
            return 1;
        }
    }
    return gPowerWhitelistNum;
}

int maliciousLoadDetect(float *fft, float pulseI, float deltaActivePower, float deltaReactivePower,
        float effU, float activePower, float reactivePower, DateStruct *date, char *errMsg) {

    if (deltaActivePower < 0)
        return 0;
    float stActivePower = getStandardPower(deltaActivePower, effU);
    if (stActivePower < gMinPower) {
        if (errMsg != NULL) {
            sprintf(errMsg, "pc:%.2f %.2f", deltaActivePower, effU);
        }
        return 0;
    }

    if (isInMaliciousLoadWhitelist(stActivePower)) {
        if (errMsg != NULL) {
            sprintf(errMsg, "wl:%.2f", stActivePower);
        }
        return 0;
    }

    float powerFactor = deltaActivePower
            / (sqrt(deltaActivePower * deltaActivePower + deltaReactivePower * deltaReactivePower));
    int isHeatingDevice = 0;
    if (powerFactor >= 0.97f && pulseI < 1.11f && fft[0] / (fft[1] + LF) >= 20) {
        isHeatingDevice = 1;
    }

    if (isHeatingDevice) {
        //step:空调制热模式误报
        //线路无功功率不足200w,空调不在运行,直接报警
        if (getStandardPower(reactivePower, effU) < 200) {
            return 1;
        }

        //判断月份是否在寒冷的月份(11-3)
        int month = date->mon;
        if (month > 3 && month < 11) { //非寒冷月份不存在热空调，直接报警
            return 1;
        } else {
            if ((fft[1] + fft[2] + fft[3] + fft[4]) / fft[0] >= 0.1) {
                if (errMsg != NULL) {
                    sprintf(errMsg, "fc:%.2f %.2f %.2f %.2f %.2f", fft[0], fft[1], fft[2], fft[3], fft[4]);
                }
                return 0;
            } else {
                if (deltaReactivePower <= 80 && (activePower - deltaActivePower) < 1000) {
                    if (errMsg != NULL) {
                        sprintf(errMsg, "ad:%.2f %.2f %.2f", activePower, deltaActivePower,
                                deltaReactivePower);
                    }
                    return 0;
                }
            }

        }

    }
    return 0;
}
