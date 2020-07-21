#include<math.h>
#include<stddef.h>

// 过零检测
 int zeroCrossPoint(float sample[], int start, int end) {
    for (int i = start; i < end; i++) {
        if (sample[i] <= 0.0f && sample[i + 1] > 0.0f) {
            if (i - 1 >= start && i + 2 < end) {
                if (sample[i - 1] < 0.0f && sample[i + 2] > 0.0f)
                    return i + 1;
            } else {
                return i + 1;
            }
        }
    }
    return -1;
}

 float nilmuMean(float inputs[], int start, int len) {
    float sum = 0;
    int end = start + len;
    for (int i = start; i < end; i++)
    sum += inputs[i];

    return sum / len;
}

/**
 * 有效值
 */
 float nilmEffectiveValue(float inputs[], int start, int len) {
    float sumOfSquares = 0;
    int end = start + len;
    for (int i = start; i < end; i++)
    sumOfSquares += inputs[i] * inputs[i];

    return (float) sqrt(sumOfSquares / len);
}

/**
 * 有功功率，可正可负
 */
 float nilmActivePower(float current[], int indexI, float voltage[], int indexU,
        int len) {

    float activePower = 0;
    for (int i = 0; i < len; i++) {
        activePower += current[indexI + i] * voltage[indexU + i];
    }
    activePower /= len;
    return activePower; // > 0 ? activePower : -activePower;
}

/**
 *
 * @param inputs
 * @param len
 * @param absThresh
 *            绝对值的波动，比如设为20，则20以内的波动无论比值波动多少都不计
 * @param relaRatio
 *            相对比值波动，至少波动比例，百分比
 * @return
 */
 int nilmIsStable(float inputs[], int len, float absThresh, int relativeRatio) {

    float average = 0, absAverage = 0;
    for (int i = 0; i < len; i++) {
        average += inputs[i];
    }
    average /= len;
    absAverage = average >= 0 ? average : -average;
    for (int i = 0; i < len; i++) {
        float absDelta = inputs[i] - average;
        absDelta = absDelta >= 0 ? absDelta : -absDelta;

        if (absDelta > absThresh && absDelta * 100 > absAverage * relativeRatio) {
            return 0;
        }
    }
    return 1;
}

/**
 *
 * @param inputs
 * @param inputLen
 *            输入的长度
 * @param start
 *            计算的起点
 * @param len
 *            要计算的长度
 * @param ignoreThresh
 *            忽略多大的小波动
 * @return 最大波动的百分比
 */
 float nilmGetFluctuation(float inputs[], int inputLen, int start, int len,
        float ignoreThresh) {
    if (inputLen < len)
    return 0;
    float average = 0, absAverage = 0;
    for (int i = start; i < start + len; i++) {
        average += inputs[i % inputLen];
    }
    average /= len;
    absAverage = average >= 0 ? average : -average;
    float maxFluctuation = 0, tmp = 0;
    for (int i = start; i < start + len; i++) {
        float absDelta = inputs[i % inputLen] - average;
        absDelta = absDelta > 0 ? absDelta : -absDelta;
        tmp = absDelta / absAverage;
        if (absDelta >= ignoreThresh && tmp > maxFluctuation) {
            maxFluctuation = tmp;
        }
    }
    return maxFluctuation * 100;
}

float nilmAbs(float val) {
    return val >= 0 ? val : -val;
}

 float nilmSqrt(float val) {
    return (float) sqrt(val);
}

 float nilmPower(float base, float index) {
    return (float) pow(base, index);
}

/**
 * *为nilm量身订做
 *
 * @param oldData
 * @param newData
 * @param delta
 * @param len
 *            delta's length
 * @return
 */
 int calDelta(float oldU[], float newU[], float oldI[], float newI[],
        float delta[], int len) {

    int zero1 = zeroCrossPoint(oldU, 0, len);
    int zero2 = zeroCrossPoint(newU, 0, len);

    if (zero1 < 0 || zero2 < 0 || zero1 > 128 || zero2 > 128)
    return -1;

    for (int i = 0; i < len; i++) {
        delta[i] = newI[zero2++] - oldI[zero1++];
    }
    return 0;
}

// 按照顺序插入，最新插入的数据排在buff后面
 void insertFifoBuff(float buff[], int buffSize, float newData[], int dataLen) {

    int end = buffSize - dataLen;
    for (int i = 0; i < end; i++) {
        buff[i] = buff[i + dataLen];
    }

    for (int i = 0; i < dataLen; i++) {
        buff[buffSize - dataLen + i] = newData[i];
    }
}

// 只插入一条，最新插入的数据排在buff后面
 void insertFifoBuffOne(float buff[], int buffSize, float newData) {

    buff[buffSize - 1] = newData;
    int end = buffSize - 1;
    for (int i = 0; i < end; i++) {
        buff[i] = buff[i + 1];
    }
}

/**
 * 欧式距离相似度
 */
 double euclideanDis(float a[], float b[], int start, int len) {
    if (a == NULL || b == NULL) {
        return -1;
    }

    float ret = 0;
    int end = start + len;
    for (int i = start; i < end; i++) {
        ret += (a[i] - b[i]) * (a[i] - b[i]);
    }
    return sqrt(ret);
}

 /**
 * cosine相似度,[-1,1]
 */
double cosine(float a[], float b[], int start, int len) {
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

    return (numerator / (sqrt(denominatorA) * sqrt(denominatorB)));
}
