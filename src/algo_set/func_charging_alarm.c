#include "func_charging_alarm.h"
#include "algo_base_struct.h"
#include "math_utils.h"
#include "log_utils.h"
#include "algo_set_build.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

static int gSensitivity[CHANNEL_NUM];
static float gPresetMinPower[CHANNEL_NUM]; //-1表示未配置
static float gPresetMaxPower[CHANNEL_NUM];

void setMinChargingDevicePower(int channel, float power) {
    if (power < 50) {
        return;
    }
    gPresetMinPower[channel] = power;
}

void setMaxChargingDevicePower(int channel, float power) {
    gPresetMaxPower[channel] = power;
}

int initFuncChargingAlarm(void) {

    for (int i = 0; i < CHANNEL_NUM; i++) {
        gSensitivity[i] = CHARGING_ALARM_SENSITIVITY_MEDIUM;
        gPresetMinPower[i] = -1;
        gPresetMaxPower[i] = -1;
    }
    return 0;
}

static float chargers[][5] = { { 1.24, 0.89, 0.413, 0.24, 0.321 }, //wangao
        { 1.272, 0.990, 0.652, 0.403, 0.297 }, { 1.302, 1.032, 0.618, 0.385, 0.337 }, { 1.268, 1.037, 0.663,
                0.390, 0.323 }, //200w
        { 0.721, 0.587, 0.399, 0.265, 0.238 }, { 0.731, 0.625, 0.400, 0.269, 0.209 }, //100w
//        {0.26466367,0.22620744,0.19432415,0.13409556,0.099954225},//误报
//        { 3.38, 0.467, 0.124, 0.291, 0.133 }, { 0.514, 0.052, 0.038, 0.008, 0 }, { 0.815, 0.627, 0.352, 0.108,
//                0.067 }, { 1.14, 0.63, 0.35, 0.1, 0.06 }, { 0.868, 0.774, 0.607, 0.399, 0.201 } //test
        };

/**
 * return: 1 检测到充电设备
 * cur:电流采样序列
 * curStart:序列起始位置.要保证从起始位置后至少有128个采样点
 * vol:电压采样序列
 * volStart:序列起始位置.要保证从起始位置后至少有128个采样点
 * fft:谐波数组,大小为5,包含一三五七九次谐波分量
 * pulseI:启动冲量
 * deltaEffI:差分有效电流
 * deltaEffU:差分有效电压
 * deltaActivePower:差分有功功率
 * deltaReactivePower:差分无功功率
 */
int chargingDetect(int channel, float *fft, float pulseI, float curSamplePulse, float deltaActivePower,
        float deltaReactivePower, int startupTime, WaveFeature *wf, char *errMsg) {

    if (deltaActivePower < 0)
        return 0;
    float pulseIThresh = 1.5f;
    float thetaThresh = 13;
    int minExtreme = 2, maxExtreme = 2;
    int minFlat = 18;
    float minActivePower = 85, minReactivePower = 100, maxActivePower = 280;
    float maxDeltaRatio = 0.8f;

    switch (gSensitivity[channel]) {
    case CHARGING_ALARM_SENSITIVITY_LOW: //低灵敏度
        pulseIThresh = 2.0f; //越大越严
        thetaThresh = 9; //越小越严
        maxExtreme = 2; //越小越严
        minFlat = 18; //越大越严
        minActivePower = 100; //越大越严
        minReactivePower = 100; //越大越严
        maxDeltaRatio = 0.75f; //越小越严
        break;
    case CHARGING_ALARM_SENSITIVITY_MEDIUM:
        pulseIThresh = 0.9f; //正常为大于等于1
        thetaThresh = 10.5;
        maxExtreme = 4;
        minFlat = 16;
        minActivePower = 90;
        minReactivePower = 100;
        maxDeltaRatio = 0.8f;
        break;
    case CHARGING_ALARM_SENSITIVITY_HIGH: //高灵敏度
        pulseIThresh = 0.0f;
        thetaThresh = 13;
        maxExtreme = 7;
        minFlat = 13;
        minActivePower = 80;
        minReactivePower = 90;
        maxDeltaRatio = 0.9f;
        break;

    default:
        break;
    }

    //功率阈值,手动配置更新
    if (gPresetMinPower[channel] > 0) {
        minActivePower = gPresetMinPower[channel];
        minReactivePower = minActivePower * 1.2f;
        if (minReactivePower > 130) {
            minReactivePower = 130;
        }
    }
    if (gPresetMaxPower[channel] > 0) {
        maxActivePower = gPresetMaxPower[channel];
    }

    float maxPulse = pulseI > curSamplePulse ? pulseI : curSamplePulse;
    if (deltaActivePower < minActivePower || deltaActivePower > maxActivePower || pulseI < pulseIThresh
            || (fft[1] + fft[2] + fft[3] + fft[4]) / (fft[0] + 0.0001f) < 1 || fft[1] * 1.05f > fft[0]
            || (startupTime >= 5 && maxPulse < 3.0f) || deltaReactivePower < minReactivePower) {
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("da=%.0f pi=%.1f csp=%.1f str=%d rp=%.0f fr=%.2f f1=%.2f f3=%.2f\n", deltaActivePower,
                    pulseI, curSamplePulse, startupTime, deltaReactivePower,
                    (fft[1] + fft[2] + fft[3] + fft[4]) / (fft[0] + 0.0001f), fft[0], fft[1]);
        }
#endif
        return 0;
    }

    //余弦相似度
    float minTheta = 180;
    for (int i = 0; i < sizeof(chargers) / 20; i++) {
        float theta = getCosineSimilarity(chargers[i], fft, 0, 5);
        if (theta < minTheta) {
            minTheta = theta;
        }
    }
    if (minTheta > thetaThresh) {
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("th=%.1f", minTheta);
        }
#endif
        return 0;
    }

    if (wf->flatNum < minFlat || wf->extremeNum < minExtreme || wf->extremeNum > maxExtreme) {
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("fn=%d en=%d\r\n", wf->flatNum, wf->extremeNum);
        }
#endif
        return 0;
    }

    //排除斩波电路干扰. 斩波会从平肩直接跳变到最大值，充电会有个上升过程
    if (wf->maxDelta >= wf->maxValue * maxDeltaRatio) {
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("mad=%.2f mav=%.2f\r\n", wf->maxDelta, wf->maxValue);
        }
#endif
        return 0;
    }

    //极值点左右点数
    int checkPass = 1;
    int unmatchCounter = 0;
    switch (gSensitivity[channel]) {
    case CHARGING_ALARM_SENSITIVITY_HIGH:
        checkPass = 1;
        break;
    case CHARGING_ALARM_SENSITIVITY_MEDIUM: //中灵敏度
    case CHARGING_ALARM_SENSITIVITY_LOW: //低灵敏度
        unmatchCounter = (wf->maxLeftNum > wf->maxRightNum) + (wf->minLeftNum > wf->minRightNum);
        if (unmatchCounter >= 2 || wf->maxLeftPointNum >= wf->maxRightPointNum
                || wf->minLeftPointNum >= wf->minRightPointNum) {
            checkPass = 0;
        }
        break;
    default:
        break;
    }
    if (checkPass == 0) {
#if OUTLOG_ON
        if (outprintf != NULL) {
            outprintf("near=%d %d %d %d | %d %d %d %d\r\n", wf->maxLeftNum, wf->maxRightNum, wf->minLeftNum,
                    wf->minRightNum, wf->maxLeftPointNum, wf->maxRightPointNum, wf->minLeftPointNum,
                    wf->minRightPointNum);
        }
#endif
        return 0;
    }
#if OUTLOG_ON
    if (outprintf != NULL) {
        outprintf("charging detected fn=%d en=%d\r\n", wf->flatNum, wf->extremeNum);
    }
#endif
    return 1;
}

static const int MODE_MAX = 2;
void setChargingAlarmSensitivity(int channel, int sensitivity) {
    if (sensitivity >= -1 && sensitivity <= MODE_MAX) {
        gSensitivity[channel] = sensitivity;
    }
}

int getChargingAlarmSensitivity(int channel) {
    return gSensitivity[channel];
}

int similarityTest(void) {

#if LOG_ON == 1
    for (int i = 0; i < sizeof(chargers) / 20; i++)
        for (int j = 0; j < sizeof(chargers) / 20; j++) {

            float s = getCosineSimilarity(chargers[i], chargers[j], 0, 5);
            printf("n=%d %dvs%d: theta=%.2f\n", sizeof(chargers) / 20, i, j, s);
        }
#endif
    return 0;
}
