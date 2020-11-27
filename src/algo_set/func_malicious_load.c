#include "algo_base_struct.h"
#include "data_structure_utils.h"
#include "func_malicious_load.h"
#include "power_utils.h"
#include "time_utils.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

static const int MODE_MAX = 2;
static int gMode = MALI_LOAD_SENSITIVITY_MEDIUM;
void setMaliLoadAlarmMode(int mode) {
    if (mode >= -1 && mode <= MODE_MAX) {
        gMode = mode;
    }
}

int getMaliLoadAlarmMode(void) {
    return gMode;
}

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
        float effU, float activePower, float reactivePower, WaveFeature *deltaWf, DateStruct *date,
        char *errMsg) {

    if (deltaActivePower < 0)
        return 0;

    int minFlatNum = 15;
    float minPf = 0.97f, maxPulseI = 1.09f, minFft1d3 = 18, maxHarmRatio = 0.1f, minAcReactivePower = 200;
    switch (gMode) {
    case MALI_LOAD_SENSITIVITY_LOW: //低灵敏度
        minPf = 0.985f; //越大越严
        minFlatNum = 15; //越大越严
        maxPulseI = 1.04f; //越小越严
        minFft1d3 = 20; //越大越严
        maxHarmRatio = 0.1f; //越小越严
        minAcReactivePower = 250; //越大越严
        break;
    case MALI_LOAD_SENSITIVITY_MEDIUM:
        minPf = 0.97f;
        minFlatNum = 14;
        maxPulseI = 1.09f;
        minFft1d3 = 17.5f;
        maxHarmRatio = 0.12f;
        minAcReactivePower = 205;
        break;
    case MALI_LOAD_SENSITIVITY_HIGH: //高灵敏度
        minPf = 0.96f;
        minFlatNum = 13;
        maxPulseI = 1.12f;
        minFft1d3 = 16;
        maxHarmRatio = 0.13f;
        minAcReactivePower = 190;
        break;

    default:
        break;
    }

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

    //半波设备
    if (deltaWf->extremeNum == 1 && deltaWf->flatNum >= minFlatNum) {
        if (errMsg != NULL) {
            sprintf(errMsg, "hfd: %d %d", deltaWf->extremeNum, deltaWf->flatNum);
        }
        return 2;
    }

    float powerFactor = deltaActivePower
            / (sqrt(deltaActivePower * deltaActivePower + deltaReactivePower * deltaReactivePower));
    int isHeatingDevice = 0;
    if (powerFactor >= minPf && pulseI < maxPulseI && fft[0] / (fft[1] + LF) >= minFft1d3) {
        isHeatingDevice = 1;
    }

    if (isHeatingDevice) {
        //step:空调制热模式误报
        //线路无功功率不足200w,空调不在运行,直接报警
        if (getStandardPower(reactivePower, effU) < minAcReactivePower) {
            return 1;
        }

        //判断月份是否在寒冷的月份(11-3)
        int month = date->mon;
        if (month > 3 && month < 11) { //非寒冷月份不存在热空调，直接报警
            return 1;
        } else {
            if ((fft[1] + fft[2] + fft[3] + fft[4]) / fft[0] >= maxHarmRatio) {
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
        return 1;
    }
    return 0;
}
