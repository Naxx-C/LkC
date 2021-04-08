#include "func_dorm_converter.h"
#include "algo_base_struct.h"
#include "algo_set_build.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static int gSensitivity[CHANNEL_NUM];
static float gPresetMinPower[CHANNEL_NUM];
static float gPresetMaxPower[CHANNEL_NUM];

void setMinDormConverterPower(int channel, float power) {
    gPresetMinPower[channel] = power;

}
void setMaxDormConverterPower(int channel, float power) {
    gPresetMaxPower[channel] = power;
}

int initFuncDormConverter(void) {

    for (int i = 0; i < CHANNEL_NUM; i++) {
        gSensitivity[i] = DORM_CONVERTER_SENSITIVITY_MEDIUM;
        gPresetMinPower[i] = -1;
        gPresetMaxPower[i] = -1;
    }
    return 0;
}

//调压器调整过程中监测
int dormConverterAdjustingCheck(int channel, float activePower, float reactivePower, WaveFeature *wf,
        char *errMsg) {
    int minExtreme = 2, maxExtreme = 3;
    int minFlat = 14;
    float minActivePower = 150, minReactivePower = 150, maxActivePower = 3000;
    float minDeltaRatio = 0.8f;

    switch (gSensitivity[channel]) {
    case DORM_CONVERTER_SENSITIVITY_LOW: //低灵敏度
        minExtreme = 2; //越大越严
        maxExtreme = 2; //越小越严
        minFlat = 18; //越大越严
        minActivePower = 180; //越大越严
        minReactivePower = 150; //越大越严
        minDeltaRatio = 0.9f; //越大越严
        break;
    case DORM_CONVERTER_SENSITIVITY_MEDIUM:
        minExtreme = 2;
        maxExtreme = 3;
        minFlat = 14;
        minActivePower = 150;
        minReactivePower = 150;
        minDeltaRatio = 0.8f;
        break;
    case DORM_CONVERTER_SENSITIVITY_HIGH: //高灵敏度
        minExtreme = 2;
        maxExtreme = 4;
        minFlat = 10;
        minActivePower = 120;
        minReactivePower = 120;
        minDeltaRatio = 0.8f;
        break;

    default:
        break;
    }

    //功率阈值,手动配置更新
    if (gPresetMinPower[channel] > 0) {
        minActivePower = gPresetMinPower[channel];
    }
    if (gPresetMaxPower[channel] > 0) {
        maxActivePower = gPresetMaxPower[channel];
    }

    if (wf->flatNum >= minFlat && wf->maxDelta >= wf->maxValue * minDeltaRatio
            && activePower >= minActivePower && activePower <= maxActivePower
            && reactivePower >= minReactivePower && wf->extremeNum >= minExtreme
            && wf->extremeNum <= maxExtreme) {
        return 1;
    }
    return 0;
}

int dormConverterDetect(int channel, float deltaActivePower, float deltaReactivePower, WaveFeature *wf,
        char *errMsg) {

    int minExtreme = 2, maxExtreme = 3;
    int minFlat = 14;
    float minActivePower = 100, minReactivePower = 100;
    float minDeltaRatio = 0.8f;

    switch (gSensitivity[channel]) {
    case DORM_CONVERTER_SENSITIVITY_LOW: //低灵敏度
        minExtreme = 2; //越大越严
        maxExtreme = 2; //越小越严
        minFlat = 16; //越大越严
        minActivePower = 130; //越大越严
        minReactivePower = 100; //越大越严
        minDeltaRatio = 0.9f; //越大越严
        break;
    case DORM_CONVERTER_SENSITIVITY_MEDIUM:
        minExtreme = 2;
        maxExtreme = 3;
        minFlat = 12;
        minActivePower = 100;
        minReactivePower = 80;
        minDeltaRatio = 0.85f;
        break;
    case DORM_CONVERTER_SENSITIVITY_HIGH: //高灵敏度
        minExtreme = 2;
        maxExtreme = 4;
        minFlat = 8;
        minActivePower = 100;
        minReactivePower = 50;
        minDeltaRatio = 0.8f;
        break;

    default:
        break;
    }

    //功率阈值,手动配置更新
    if (gPresetMinPower[channel] > 0) {
        minActivePower = gPresetMinPower[channel];
    }

    float maxActivePower = 3000.0f;
    if (gPresetMaxPower[channel] > 0) {
        maxActivePower = gPresetMaxPower[channel];
    }

    //功率判断
    if (deltaActivePower < minActivePower || deltaActivePower > maxActivePower || deltaReactivePower < minReactivePower) {
        if (errMsg != NULL) {
            sprintf(errMsg, "dap=%.2f drp=%.2f", deltaActivePower, deltaReactivePower);
        }
#if TUOQIANG_DEBUG
#if OUTLOG_ON
        outprintf("dap=%.2f drp=%.2f", deltaActivePower, deltaReactivePower);
#endif
#endif
        return 0;
    }

    //波形判断
    if (wf->flatNum < minFlat || wf->extremeNum < minExtreme || wf->extremeNum > maxExtreme) {
        if (errMsg != NULL) {
            sprintf(errMsg, "fn=%d en=%d", wf->flatNum, wf->extremeNum);
        }
#if TUOQIANG_DEBUG
#if OUTLOG_ON
        outprintf("fn=%d en=%d", wf->flatNum, wf->extremeNum);
#endif
#endif
        return 0;
    }

    //排除充电干扰. 斩波会从平肩直接跳变到最大值，充电会有个上升过程
    if (wf->maxDelta < wf->maxValue * minDeltaRatio && wf->minNegDelta > wf->minNegValue * minDeltaRatio) {
        if (errMsg != NULL) {
            sprintf(errMsg, "mad=%.2f mav=%.2f", wf->maxDelta, wf->maxValue);
        }
#if TUOQIANG_DEBUG
#if OUTLOG_ON
        outprintf("mad=%.2f mav=%.2f", wf->maxDelta, wf->maxValue);
#endif
#endif
        return 0;
    }
    if (errMsg != NULL) {
        sprintf(errMsg, "dorm converter detected");
    }
    return 1;
}

static const int MODE_MAX = 2;
void setDormConverterAlarmSensitivity(int channel, int sensitivity) {
    if (sensitivity >= -1 && sensitivity <= MODE_MAX) {
        gSensitivity[channel] = sensitivity;
    }
}

int getDormConverterAlarmSensitivity(int channel) {
    return gSensitivity[channel];
}
