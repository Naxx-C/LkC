#ifndef __SHORTCIRCUIT_ALGO
#define __SHORTCIRCUIT_ALGO

#define _WINDOWS

float arcuMean(float *inputs, int len);
float arcuEffectiveValue(float* inputs, int len);
float arcuThreshAverage(float* inputs, int len, float thresh);
float rangeAvg(float * inputs, float min, float max, int start, int end);
char arcuIsConsistent(float *current, int length, float direction, float thresh, int startIndex, int checkNum);
int getExtremeIndex(float * current, int arcFaultIndex, int end);
int rangeCounter(float* current, int len, float max, float min);
int arcuGetWidth(float* current, int currentLen, int arcFaultIndex);
int arcuGetHealth(float* delta, float * absDelta, int len, float thresh);
int arcuExtremeInRange(float* current, int currentLen, int arcFaultIndex, int range, float averageDelta);
int arcuIsMaxInRange(float * current, int currentLen, int arcFaultIndex, int range);
int arcuGetBigNum(float* in, int len, float thresh, float ratio);
char arcuIsStable(float * inputs, int len, float absThresh, int relativeRatio);
float arcuAbs(float val);
float arcuSqrt(float val);
#endif
