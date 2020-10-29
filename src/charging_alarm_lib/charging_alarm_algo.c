#include "charging_alarm_algo.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#define PI 3.14159265f
const static char VERSION[] = { 1, 0, 0, 1 };

typedef struct {
    int flatNum; //平肩数目
    int extremeNum; //极值点数目
    int maxLeftNum; //极值点左右的点数
    int maxRightNum;
    int minLeftNum;
    int minRightNum;
} ChargingWaveFeature;

static int gMode = CHARGING_ALARM_SENSITIVITY_MEDIUM;

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

static float chargers[][5] = { { 1.24, 0.89, 0.413, 0.24, 0.321 }, //wangao
        { 1.272, 0.990, 0.652, 0.403, 0.297 }, { 1.302, 1.032, 0.618, 0.385, 0.337 }, { 1.268, 1.037, 0.663,
                0.390, 0.323 }, //200w
        { 0.721, 0.587, 0.399, 0.265, 0.238 }, { 0.731, 0.625, 0.400, 0.269, 0.209 }, //100w
//        { 3.38, 0.467, 0.124, 0.291, 0.133 }, { 0.514, 0.052, 0.038, 0.008, 0 }, { 0.815, 0.627, 0.352, 0.108,
//                0.067 }, { 1.14, 0.63, 0.35, 0.1, 0.06 }, { 0.868, 0.774, 0.607, 0.399, 0.201 } //test
        };

static float calCosineSimilarity(float a[], float b[], int start, int len) {
    if (a == NULL || b == NULL) {
        return -1;
    }

    float numerator = 0;
    float denominatorA = 0;
    float denominatorB = 0;
    int end = start + len;
    for (int i = start; i < end; i++) {
        numerator += a[i] * b[i];
        denominatorA += a[i] * a[i];
        denominatorB += b[i] * b[i];
    }

    float cosine = (numerator / (sqrt(denominatorA) * sqrt(denominatorB)));

    float cosineSimilarity = (float) (acos(cosine) * 180 / PI);
    return cosineSimilarity;
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
float calActivePower(float current[], int indexI, float voltage[], int indexU, int len) {

    float activePower = 0;
    for (int i = 0; i < len; i++) {
        activePower += current[indexI + i] * voltage[indexU + i];
    }
    activePower /= len;
    return activePower; // > 0 ? activePower : -activePower;
}

//通过视在功率计算无功功率
float getReactivePowerByS(float activePower, float apparentPower) {

    float reactivePower = sqrtf(apparentPower * apparentPower - activePower * activePower);
    return reactivePower;
}

#define BATCH 4
int getWaveFeature(float *cur, int curStart, float *vol, int volStart, float effI, float effU,
        float deltaActivePower, ChargingWaveFeature *cwf) {

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
            if (direction < 0) { // && fabs(c[i]) > averageDelta / 5
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

    cwf->extremeNum = extremeNum;
    cwf->flatNum = flatNum;
    cwf->maxLeftNum = maxLeftNum;
    cwf->maxRightNum = maxRightNum;
    cwf->minLeftNum = minLeftNum;
    cwf->minRightNum = minRightNum;
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
int isChargingDevice(float *cur, int curStart, float *vol, int volStart, float *fft, float pulseI,
        float deltaEffI, float deltaEffU, float deltaActivePower, char *errMsg) {

    if (gIsLibExpired)
        return 0;

    float pulseIThresh = 1.5f;
    float thetaThresh = 13;
    int minExtreme = 2, maxExtreme = 2;
    int minFlat = 18;
    float minActivePower = 80, minReactivePower = 100;

    switch (gMode) {
    case CHARGING_ALARM_SENSITIVITY_LOW: //低灵敏度
        pulseIThresh = 3.0f;
        thetaThresh = 10;
        maxExtreme = 2;
        minFlat = 20;
        break;
    case CHARGING_ALARM_SENSITIVITY_MEDIUM:
        pulseIThresh = 1.5f;
        thetaThresh = 13;
        maxExtreme = 4;
        minFlat = 18;
        break;
    case CHARGING_ALARM_SENSITIVITY_HIGH: //高灵敏度
        pulseIThresh = 0.0f;
        thetaThresh = 18;
        maxExtreme = 7;
        minFlat = 13;
        minActivePower = 80;
        minReactivePower = 90;
        break;

    default:
        break;

    }

    deltaActivePower =
            deltaActivePower >= 0 ? deltaActivePower : calActivePower(cur, curStart, vol, volStart, 128);
    if (deltaActivePower < minActivePower || deltaActivePower > 280 || pulseI < pulseIThresh
            || (fft[1] + fft[2] + fft[3] + fft[4]) / (fft[0] + 0.0001f) < 1) {
        if (errMsg != NULL) {
            sprintf(errMsg, "da=%.0f pi=%.1f fr=%.2f", deltaActivePower, pulseI,
                    (fft[1] + fft[2] + fft[3] + fft[4]) / (fft[0] + 0.0001f));
        }
        return 0;
    }

    deltaEffI = deltaEffI >= 0 ? deltaEffI : calEffectiveValue(cur, curStart, 128);
    deltaEffU = deltaEffU >= 0 ? deltaEffU : calEffectiveValue(vol, volStart, 128);
    float reactivePower = getReactivePowerByS(deltaActivePower, deltaEffI * deltaEffU);

    //无功功率判断
    if (reactivePower < minReactivePower) {
        if (errMsg != NULL) {
            sprintf(errMsg, "rp=%.0f", reactivePower);
        }
        return 0;
    }

    //余弦相似度
    float minTheta = 180;
    for (int i = 0; i < sizeof(chargers) / 20; i++) {
        float theta = calCosineSimilarity(chargers[i], fft, 0, 5);
        if (theta < minTheta) {
            minTheta = theta;
        }
    }
    if (minTheta > thetaThresh) {
        if (errMsg != NULL) {
            sprintf(errMsg, "th=%.1f", minTheta);
        }
        return 0;
    }

    ChargingWaveFeature cwf;
    memset(&cwf, 0, sizeof(cwf));
    int wf = getWaveFeature(cur, curStart, vol, volStart, deltaEffI, deltaEffU, deltaActivePower, &cwf);
    if (wf < 0 || cwf.flatNum < minFlat || cwf.extremeNum < minExtreme || cwf.extremeNum > maxExtreme) {
        if (errMsg != NULL) {
            sprintf(errMsg, "fn=%d en=%d", cwf.flatNum, cwf.extremeNum);
        }
        return 0;
    }

    //极值点左右点数
    int checkPass = 1;
    switch (gMode) {
    case CHARGING_ALARM_SENSITIVITY_HIGH:
        checkPass = 1;
        break;
    case CHARGING_ALARM_SENSITIVITY_MEDIUM:
        if ((cwf.maxRightNum + cwf.maxLeftNum <= 3) || (cwf.minRightNum + cwf.minLeftNum <= 3)
                || (cwf.maxLeftNum >= cwf.maxRightNum && cwf.minLeftNum >= cwf.minRightNum)) {
            checkPass = 0;
        }
        break;
    case CHARGING_ALARM_SENSITIVITY_LOW: //低灵敏度
        if (cwf.maxRightNum < 3 || cwf.minRightNum < 3 || cwf.maxLeftNum > 1 || cwf.minLeftNum > 1) {
            checkPass = 0;
        }
        break;
    default:
        break;
    }
    if (checkPass == 0) {
        if (errMsg != NULL) {
            sprintf(errMsg, "near=%d %d %d %d", cwf.maxLeftNum, cwf.maxRightNum, cwf.minLeftNum,
                    cwf.minRightNum);
        }
        return 0;
    }

    if (errMsg != NULL) {
        sprintf(errMsg, "charging detected");
    }
    return 1;
}

static const int MODE_MAX = 2;
void setMode(int mode) {
    if (mode >= -1 && mode <= MODE_MAX) {
        gMode = mode;
    }
}

int getMode(void) {
    return gMode;
}

int getChargingAlarmAlgoVersion(void) {
    return VERSION[0] << 24 | VERSION[1] << 16 | VERSION[2] << 8 | VERSION[3];
}

int charging_alarm_main2(void) {

//    for (int i = 0; i < sizeof(chargers) / 20; i++)
//        for (int j = 0; j < sizeof(chargers) / 20; j++) {
//
//            float s = calCosineSimilarity(chargers[i], chargers[j], 0, 5);
//            printf("n=%d %dvs%d: theta=%.2f\n", sizeof(chargers) / 20, i, j, s);
//        }
    float fft[5] = { 0.731, 0.625, 0.400, 0.269, 0.209 };
    float pulseI = 1.9;
    float deltaEffI = 1;
    float deltaEffU = 220;
    float deltaActivePower = 100;
    chargingAlarmAlgoinit();
    char errMsg[50];
    memset(errMsg, 0, sizeof(errMsg));
    int iscd = isChargingDevice(NULL, 0, NULL, 0, fft, pulseI, deltaEffI, deltaEffU, deltaActivePower,
            errMsg);

    printf("%d errMsg:%s\n", iscd, errMsg != NULL ? errMsg : "ok");
    return 0;
}

extern char *APP_BUILD_DATE;
static const char *EXPIRED_DATE = "2021-06-30";
int chargingAlarmAlgoinit() {
    gIsLibExpired = isExpired(APP_BUILD_DATE, EXPIRED_DATE);
    if (gIsLibExpired) {
        setMode(CHARGING_ALARM_DISABLED);
        return 1;
    }
    return 0;
}
