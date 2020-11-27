#ifndef __NILM_UTILS
#define __NILM_UTILS

#include<math.h>
#include<stddef.h>

// 过零检测
int zeroCrossPoint(float sample[], int start, int end);

float nilmuMean(float inputs[], int start, int len);
/**
 * 有效值
 */
float nilmEffectiveValue(float inputs[], int start, int len);
/**
 * 有功功率，可正可负
 */
float nilmActivePower(float current[], int indexI, float voltage[], int indexU, int len);

float nilmGetReactivePowerByPf(float activePower, float powerFactor);
float nilmGetReactivePowerByS(float activePower, float apparentPower);

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
int nilmIsStable(float inputs[], int len, float absThresh, int relativeRatio);

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
float nilmGetFluctuation(float inputs[], int inputLen, int start, int len, float ignoreThresh);

float nilmAbs(float val);

float nilmSqrt(float val);

float nilmPower(float base, float index);
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
int calDelta(float oldU[], float newU[], float oldI[], float newI[], float delta[], int len);

// 按照顺序插入，最新插入的数据排在buff后面
void insertFifoBuff(float buff[], int buffSize, float newData[], int dataLen);

// 只插入一条，最新插入的数据排在buff后面
void insertFifoBuffOne(float buff[], int buffSize, float newData);

void insertFifoCommon(char *buff, int buffSize, char *newData, int dataLen);
/**
 * cosine相似度,[-1,1]
 */
float cosine(float a[], float b[], int start, int len);
/**
 * 欧式距离相似度
 */
double euclideanDis(float a[], float b[], int start, int len);

#endif
