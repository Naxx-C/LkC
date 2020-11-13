#include "func_dorm_converter.h"
#include "algo_base_struct.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static int gMode = DORM_CONVERTER_SENSITIVITY_MEDIUM;

//调压器调整过程中监测
int dormConverterAdjustingCheck(float activePower, float reactivePower, WaveFeature *wf, char *errMsg) {
    int minExtreme = 2, maxExtreme = 3;
    int minFlat = 14;
    float minActivePower = 150, minReactivePower = 150;
    float minDeltaRatio = 0.8f;

    switch (gMode) {
    case DORM_CONVERTER_SENSITIVITY_LOW: //低灵敏度
        minExtreme = 2; //越大越严
        maxExtreme = 2; //越小越严
        minFlat = 18; //越大越严
        minActivePower = 100; //越大越严
        minReactivePower = 100; //越大越严
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
        minFlat = 12;
        minActivePower = 120;
        minReactivePower = 120;
        minDeltaRatio = 0.8f;
        break;

    default:
        break;
    }
    if (wf->flatNum >= minFlat && wf->maxDelta >= wf->maxValue * minDeltaRatio
            && activePower >= minActivePower && reactivePower >= minReactivePower
            && wf->extremeNum >= minExtreme && wf->extremeNum <= maxExtreme) {
        return 1;
    }
    return 0;
}

int dormConverterDetect(float deltaActivePower, float deltaReactivePower, WaveFeature *wf, char *errMsg) {

    int minExtreme = 2, maxExtreme = 3;
    int minFlat = 14;
    float minActivePower = 100, minReactivePower = 100;
    float minDeltaRatio = 0.8f;

    switch (gMode) {
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
        minDeltaRatio = 0.8f;
        break;
    case DORM_CONVERTER_SENSITIVITY_HIGH: //高灵敏度
        minExtreme = 2;
        maxExtreme = 4;
        minFlat = 10;
        minActivePower = 100;
        minReactivePower = 50;
        minDeltaRatio = 0.7f;
        break;

    default:
        break;
    }

    //功率判断
    if (deltaActivePower < minActivePower || deltaReactivePower < minReactivePower) {
        if (errMsg != NULL) {
            sprintf(errMsg, "dap=%.2f drp=%.2f", deltaActivePower, deltaReactivePower);
        }
        return 0;
    }

    //波形判断
    if (wf->flatNum < minFlat || wf->extremeNum < minExtreme || wf->extremeNum > maxExtreme) {
        if (errMsg != NULL) {
            sprintf(errMsg, "fn=%d en=%d", wf->flatNum, wf->extremeNum);
        }
        return 0;
    }

    //排除充电干扰. 斩波会从平肩直接跳变到最大值，充电会有个上升过程
    if (wf->maxDelta < wf->maxValue * minDeltaRatio) {
        if (errMsg != NULL) {
            sprintf(errMsg, "mad=%.2f mav=%.2f", wf->maxDelta, wf->maxValue);
        }
        return 0;
    }
    if (errMsg != NULL) {
        sprintf(errMsg, "dorm converter detected");
    }
    return 1;
}

static const int MODE_MAX = 2;
void setDormConverterAlarmMode(int mode) {
    if (mode >= -1 && mode <= MODE_MAX) {
        gMode = mode;
    }
}

int getDormConverterAlarmMode(void) {
    return gMode;
}
