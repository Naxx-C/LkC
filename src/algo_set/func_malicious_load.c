#include "algo_base_struct.h"
#include "data_structure_utils.h"
#include "func_malicious_load.h"
#include "algo_set_build.h"
#include "power_utils.h"
#include "time_utils.h"
#include "log_utils.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

static const int MODE_MAX = 2;
static int gSensitivity[CHANNEL_NUM];
void setMaliLoadAlarmSensitivity(int channel, int sensitivity) {
    if (sensitivity >= -1 && sensitivity <= MODE_MAX) {
        gSensitivity[channel] = sensitivity;
    }
}

int getMaliLoadAlarmSensitivity(int channel) {
    return gSensitivity[channel];
}

#define POWER_WHITELIST_SIZE 10
static float gPowerWhitelist[CHANNEL_NUM][POWER_WHITELIST_SIZE] = { 0 };
static int gPowerWhitelistNum[CHANNEL_NUM];

static float gPowerMinRatio[CHANNEL_NUM];
static float gPowerMaxRatio[CHANNEL_NUM];

void setMaliLoadWhitelistMatchRatio(int channel, float minRatio, float maxRatio) {
    gPowerMinRatio[channel] = minRatio;
    gPowerMaxRatio[channel] = maxRatio;
#if OUTLOG_ON
    if (outprintf != NULL) {
        outprintf("[%d]add matchRatio [%.1f-%.1f] \r\n", channel, minRatio, maxRatio);
    }
#endif
}

static float gMinPower[CHANNEL_NUM];
void setMaliLoadMinPower(int channel, float minPower) {
    gMinPower[channel] = minPower;
}

int initFuncMaliLoad(void) {

    for (int i = 0; i < CHANNEL_NUM; i++) {
        gSensitivity[i] = MALI_LOAD_SENSITIVITY_MEDIUM;
        gPowerMinRatio[i] = 0.9f;
        gPowerMaxRatio[i] = 1.1f;
        gMinPower[i] = 200;
    }
    return 0;
}

//添加白名单功率，最多10个，超过10个自动替换最早的，重复添加的自动过滤
void addMaliciousLoadWhitelist(int channel, float power) {
    if (power <= 0)
        return;

    for (int i = 0; i < POWER_WHITELIST_SIZE; i++) {
        //已存在,返回
        if (fabs(power - gPowerWhitelist[channel][i]) < 10) {
            return;
        }
    }
    //优先找到0的进行替换
    char inserted = 0;
    for (int i = 0; i < POWER_WHITELIST_SIZE; i++) {
        //已存在,返回
        if (gPowerWhitelist[channel][i] < LF) {
            gPowerWhitelist[channel][i] = power;
            inserted = 1;
            break;
        }
    }
    if (!inserted) {
        insertFloatToBuff(gPowerWhitelist[channel], POWER_WHITELIST_SIZE, power);
    }
#if OUTLOG_ON
    if (outprintf != NULL) {
        outprintf("[%d]add %.1f to whitelist \r\n", channel, power);
    }
#endif
    if (gPowerWhitelistNum[channel] < POWER_WHITELIST_SIZE) {
        gPowerWhitelistNum[channel]++;
    }
}

//查询区间内功率最接近的白名单并移除
int removeFromMaliLoadWhitelist(int channel, float power) {
    if (power <= 0)
        return 0;

    int minDeltaIndex = -1;
    float minDelta = 10000, tmp, ratio;
    for (int i = 0; i < POWER_WHITELIST_SIZE; i++) {

        tmp = fabs(gPowerWhitelist[channel][i] - power);
        if (tmp < 10) {
            minDeltaIndex = i;
            break;
        }
        ratio = gPowerWhitelist[channel][i] / power;
        if (ratio >= gPowerMinRatio[channel] && ratio <= gPowerMaxRatio[channel]) { //匹配
            if (tmp < minDelta) {
                minDelta = tmp;
                minDeltaIndex = i;
            }
        }
    }

    if (minDeltaIndex < 0)
        return 0;

    for (int i = minDeltaIndex; i < POWER_WHITELIST_SIZE - 1; i++) {
        gPowerWhitelist[channel][i] = gPowerWhitelist[channel][i + 1];
    }
    gPowerWhitelist[channel][POWER_WHITELIST_SIZE - 1] = 0;

    if (gPowerWhitelistNum[channel] > 0) {
        gPowerWhitelistNum[channel]--;
    }
    return 1;
}

//清除所有白名单
void clearMaliLoadWhitelist(int channel) {

    for (int i = 0; i < POWER_WHITELIST_SIZE; i++) {
        gPowerWhitelist[channel][i] = 0;
    }
    gPowerWhitelistNum[channel] = 0;
}

int getMaliciousLoadWhitelist(int channel, float *outWhitelist) {
    int outIndex = 0;
    //先清0，防止调用者没有初始化0
    for (int i = 0; i < POWER_WHITELIST_SIZE; i++) {
        outWhitelist[i] = 0;
    }
    for (int i = 0; i < POWER_WHITELIST_SIZE && outIndex < 10; i++) {
        if (gPowerWhitelist[channel][i] > LF) {
            outWhitelist[outIndex++] = gPowerWhitelist[channel][i];
        }
    }
    return gPowerWhitelistNum[channel];
}

/*
 * standardPower: 换算成220v下的功率
 * realPower: 实际运行的功率，用户实际看到的
 */
static int isInMaliciousLoadWhitelist(int channel, float standardPower, float realPower) {
    if (standardPower <= 0)
        return 0;
    for (int i = 0; i < POWER_WHITELIST_SIZE; i++) {
        if (gPowerWhitelist[channel][i] < LF)
            continue;
        float ratioStandard = standardPower / gPowerWhitelist[channel][i];
        float ratioReal = realPower / gPowerWhitelist[channel][i];
        if ((ratioStandard >= gPowerMinRatio[channel] && ratioStandard <= gPowerMaxRatio[channel])
                || (ratioReal >= gPowerMinRatio[channel] && ratioReal <= gPowerMaxRatio[channel])) {
            return 1;
        }
    }
    return 0;
}

int maliciousLoadDetect(int channel, float *fft, float pulseI, float curSamplePulse, float deltaActivePower,
        float deltaReactivePower, float effU, float activePower, float reactivePower, float voltageAberrRate,
        WaveFeature *deltaWf, DateStruct *date, char *errMsg) {

    if (deltaActivePower < 0)
        return 0;

    int minFlatNum = 15;
    float minPf = 0.97f, maxPulseI = 1.07f, minFft1d3 = 18, maxHarmRatio = 0.1f, minAcReactivePower = 200;
    switch (gSensitivity[channel]) {
    case MALI_LOAD_SENSITIVITY_LOW: //低灵敏度
        minPf = 1 - deltaActivePower * 0.0015f / 100; //越大越严，根据功率调整，小功率要求更高
        minPf = minPf < 0.985f ? 0.985F : minPf;
        minFlatNum = 15; //越大越严
        maxPulseI = 1.03f; //越小越严
        minFft1d3 = 26; //越大越严
        maxHarmRatio = 0.1f; //越小越严
        minAcReactivePower = 250; //越大越严
        break;
    case MALI_LOAD_SENSITIVITY_MEDIUM:
        minPf = 1 - deltaActivePower * 0.004f / 100; //越大越严，根据功率调整，小功率要求更高
        minPf = minPf < 0.975f ? 0.975F : minPf;
        minFlatNum = 14;
        maxPulseI = 1.07f;
        minFft1d3 = 20.0f;
        maxHarmRatio = 0.12f;
        minAcReactivePower = 205;
        break;
    case MALI_LOAD_SENSITIVITY_HIGH: //高灵敏度
        minPf = 1 - deltaActivePower * 0.005f / 100; //越大越严，根据功率调整，小功率要求更高
        minPf = minPf < 0.96f ? 0.96F : minPf;
        minFlatNum = 13;
        maxPulseI = 1.13f;
        minFft1d3 = 19;
        maxHarmRatio = 0.13f;
        minAcReactivePower = 190;
        break;

    default:
        break;
    }

    //step: 附加处理
    //脉冲
    float maxCurSamplePulse = maxPulseI * 1.03f; //多3%

    //根据电压畸变率修正fft条件
    minFft1d3 -= voltageAberrRate * 1.05f;
    if (minFft1d3 > 20)
        minFft1d3 = 20;
    if (minFft1d3 < 10)
        minFft1d3 = 10;

    float stActivePower = getStandardPower(deltaActivePower, effU);
    if (stActivePower < gMinPower[channel]) {
#if TUOQIANG_DEBUG
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("pc:%.2f %.2f\r\n", deltaActivePower, effU);
        }
#endif
#endif
        return 0;
    }

    if (isInMaliciousLoadWhitelist(channel, stActivePower, deltaActivePower)) {
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("wl:%.2f\r\n", stActivePower);
        }
#endif
        return 0;
    }

//半波设备
    if (deltaWf->extremeNum == 1 && deltaWf->flatNum >= minFlatNum) {
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("hfd: %d %d\r\n", deltaWf->extremeNum, deltaWf->flatNum);
        }
#endif
        return 2;
    }

    float powerFactor = deltaActivePower
            / (sqrt(deltaActivePower * deltaActivePower + deltaReactivePower * deltaReactivePower));
    int isHeatingDevice = 0;
    if (powerFactor >= minPf && pulseI < maxPulseI && curSamplePulse < maxCurSamplePulse
            && fft[0] / (fft[1] + LF) >= minFft1d3 && deltaWf->flatNum == 0 && deltaWf->extremeNum <= 4) {
        isHeatingDevice = 1;
    } else {
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("pf:%.3f %.3f pi:%.2f csp:%.2f f:%.1f va=%.1f\r\n", powerFactor, minPf, pulseI,
                    curSamplePulse, fft[0] / (fft[1] + LF), voltageAberrRate);
        }
#endif
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
#if TMP_DEBUG
#if OUTLOG_ON
                if (outprintf != NULL) {
                    outprintf("fc:%.2f %.2f %.2f\r\n", fft[0], fft[1], fft[2]);
                }
#endif
#endif
                return 0;
            } else {
                if (deltaReactivePower <= 80 && (activePower - deltaActivePower) < 1000) {
                    if (errMsg != NULL) {
                        sprintf(errMsg, "ad:%.2f %.2f %.2f", activePower, deltaActivePower,
                                deltaReactivePower);
                    }
#if TMP_DEBUG
#if OUTLOG_ON
                    if (outprintf != NULL) {
                        outprintf("ad:%.2f %.2f %.2f\r\n", activePower, deltaActivePower, deltaReactivePower);
                    }
#endif
#endif
                    return 0;
                }
            }

        }
        return 1;
    }
    return 0;
}
