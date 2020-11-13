#include "dorm_converter_algo.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#define PI 3.14159265f
const static char VERSION[] = { 1, 0, 0, 1 };

static int gMode = DORM_CONVERTER_SENSITIVITY_MEDIUM;

static char gIsLibExpired = 1;
// prebuiltDate sample "Mar 03 2020"
static int isExpired(const char *prebuiltDate, const char *expiredDate) {
    if (prebuiltDate == NULL || expiredDate == NULL)
        return 1;

    const static char MONTH_NAME[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
            "Oct", "Nov", "Dec" };
    char monthString[4] = { 0 };
    int prebuiltYear = 2020, prebuiltMonth = 4, prebuiltDay = 1;
    int expiredYear = 2020, expiredMonth = 1, expiredDay = 1;
    sscanf(prebuiltDate, "%s %d %d", monthString, &prebuiltDay, &prebuiltYear);

    for (int i = 0; i < 12; i++) {
        if (strncasecmp(monthString, MONTH_NAME[i], 3) == 0) {
            prebuiltMonth = i + 1;
            break;
        }
    }

    sscanf(expiredDate, "%d-%d-%d", &expiredYear, &expiredMonth, &expiredDay);
    int score = (expiredYear - prebuiltYear) * 31 * 12 + (expiredMonth - prebuiltMonth) * 31
            + (expiredDay - prebuiltDay);
    //得分小于0为过期
    return score < 0;
}

/**
 * 有效值
 */
static float calEffectiveValue(float inputs[], int start, int len) {
    float sumOfSquares = 0;
    int end = start + len;
    for (int i = start; i < end; i++)
        sumOfSquares += inputs[i] * inputs[i];

    return (float) sqrt(sumOfSquares / len);
}

/**
 * 有功功率，可正可负
 */
static float calActivePower(float current[], int indexI, float voltage[], int indexU, int len) {

    float activePower = 0;
    for (int i = 0; i < len; i++) {
        activePower += current[indexI + i] * voltage[indexU + i];
    }
    activePower /= len;
    return activePower; // > 0 ? activePower : -activePower;
}

//通过视在功率计算无功功率
static float getReactivePowerByS(float activePower, float apparentPower) {

    float reactivePower = sqrtf(apparentPower * apparentPower - activePower * activePower);
    return reactivePower;
}

#define BATCH 4
int getDormWaveFeature(float *cur, int curStart, float *vol, int volStart, float effI, float effU,
        float deltaActivePower, DormWaveFeature *cwf) {

    if (cwf == NULL || cur == NULL || vol == NULL)
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
    float valueThresh = max / 9, deltaThresh = ad / 2;
    for (int i = 0; i < 32; i++) {
        // 周边至少一个点也是平肩
        if ((i >= 1 && fabs(c[i]) < valueThresh && fabs(c[i - 1]) < valueThresh
                && fabs(c[i] - c[i - 1]) < deltaThresh)
                || (i <= 31 && fabs(c[i]) < valueThresh && fabs(c[i + 1]) < valueThresh
                        && fabs(c[i + 1] - c[i]) < deltaThresh)) {
            flatNum++;
            flatBitmap = flatBitmap | (0x1 << i);
        }
    }
    // 极值点个数，极值点非平肩
    for (int i = 0; i < 32; i++) {
        if (c[i + 1] - c[i] > 0 && (flatBitmap & ((0x1 << (i + 1)) | (0x1 << i))) == 0) {
            if (direction < 0) {
                extremeNum++;
            }
            direction = 1;
        } else if (c[i + 1] - c[i] < 0 && (flatBitmap & ((0x1 << (i + 1)) | (0x1 << i))) == 0) {
            if (direction > 0) {
                extremeNum++;
            }
            direction = -1;
        }
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

    cwf->extremeNum = extremeNum;
    cwf->flatNum = flatNum;
    cwf->maxLeftNum = maxLeftNum;
    cwf->maxRightNum = maxRightNum;
    cwf->minLeftNum = minLeftNum;
    cwf->minRightNum = minRightNum;
    cwf->maxDelta = maxDelta;
    cwf->maxValue = maxValue;
    return 0;
}

/**
 * cur:电流采样序列
 * curStart:序列起始位置.要保证从起始位置后至少有128个采样点
 * vol:电压采样序列
 * volStart:序列起始位置.要保证从起始位置后至少有128个采样点
 * fft:谐波数组,大小为5,包含一三五七九次谐波分量
 * pulseI:启动冲量
 * deltaEffI:差分有效电流
 * deltaEffU:差分有效电压
 * deltaActivePower:差分有功功率
 */
int isDormConverter(float *cur, int curStart, float *vol, int volStart, float deltaEffI, float deltaEffU,
        float deltaActivePower, char *errMsg) {

    if (gIsLibExpired)
        return 0;

    int minExtreme = 2, maxExtreme = 3;
    int minFlat = 14;
    float minActivePower = 100, minReactivePower = 100;
    float minDeltaRatio = 0.8f;

    switch (gMode) {
    case DORM_CONVERTER_SENSITIVITY_LOW: //低灵敏度
        minExtreme = 2;//越大越严
        maxExtreme = 2;//越小越严
        minFlat = 16;//越大越严
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

    deltaActivePower =
            deltaActivePower >= 0 ? deltaActivePower : calActivePower(cur, curStart, vol, volStart, 128);
    if (deltaActivePower < minActivePower) {
        if (errMsg != NULL) {
            sprintf(errMsg, "da=%.0f", deltaActivePower);
        }
        return 0;
    }

    deltaEffI = deltaEffI >= 0 ? deltaEffI : calEffectiveValue(cur, curStart, 128);
    deltaEffU = deltaEffU >= 0 ? deltaEffU : calEffectiveValue(vol, volStart, 128);
    float deltaReactivePower = getReactivePowerByS(deltaActivePower, deltaEffI * deltaEffU);

    //无功功率判断
    if (deltaReactivePower < minReactivePower) {
        if (errMsg != NULL) {
            sprintf(errMsg, "rp=%.0f", deltaReactivePower);
        }
        return 0;
    }

    DormWaveFeature cwf;
    memset(&cwf, 0, sizeof(cwf));
    int wf = getDormWaveFeature(cur, curStart, vol, volStart, deltaEffI, deltaEffU, deltaActivePower, &cwf);
    if (wf < 0 || cwf.flatNum < minFlat || cwf.extremeNum < minExtreme || cwf.extremeNum > maxExtreme) {
        if (errMsg != NULL) {
            sprintf(errMsg, "fn=%d en=%d", cwf.flatNum, cwf.extremeNum);
        }
        return 0;
    }

    //排除斩波电路干扰. 斩波会从平肩直接跳变到最大值，充电会有个上升过程
    if (cwf.maxDelta < cwf.maxValue * minDeltaRatio) {
        if (errMsg != NULL) {
            sprintf(errMsg, "mad=%.2f mav=%.2f", cwf.maxDelta, cwf.maxValue);
        }
        return 0;
    }
    if (errMsg != NULL) {
        sprintf(errMsg, "dorm converter detected");
    }
    return 1;
}

static const int MODE_MAX = 2;
void setDormConverterMode(int mode) {
    if (mode >= -1 && mode <= MODE_MAX) {
        gMode = mode;
    }
}

int getDormConverterMode(void) {
    return gMode;
}

int getDormConverterAlgoVersion(void) {
    return VERSION[0] << 24 | VERSION[1] << 16 | VERSION[2] << 8 | VERSION[3];
}

extern char *APP_BUILD_DATE;
static const char *EXPIRED_DATE = "2021-06-30";
int dormConverterAlgoInit() {
    gIsLibExpired = isExpired(APP_BUILD_DATE, EXPIRED_DATE);
    if (gIsLibExpired) {
        setDormConverterMode(DORM_CONVERTER_DISABLED);
        return 1;
    }
    return 0;
}
