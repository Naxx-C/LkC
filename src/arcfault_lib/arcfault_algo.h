#ifndef __ARCFAULT_ALGO
#define __ARCFAULT_ALGO
char arcAnalyze(float* current, int length, int* outArcNum);
char arcAnalyzeInner(float *current, const int length, float effCurrent, float *oddFft, int *outArcNum);
void setArcMinExtremeDis(int minExtremeDis);
void setArcMinWidth(int minWidth);
void setArcCallPeriod(int callPeriod);
void setArcDelayCheckTime(int delayCheckTime);
void setArcResJumpRatio(float resJumpRatio);
void setArcAlarmThresh(int alarmThresh);
void setArcDutyRatioThresh(int dutyRatioThresh);
void setArcArc2InWaveRatioThresh(int arc2NumRatioThresh);
void setArcMaxSeriesThresh(int maxSeriesThresh);
void setArcResFollowJumpMaxRatio(float resFollowJumpMaxRatio);
void setArcInductJumpRatio(float inductJumpRatio);
void setArcResJumpThresh(float resJumpThresh);
void setArcInductJumpThresh(float inductJumpThresh);
void setArcInductMaxJumpRatio(float inductMaxJumpRatio);
void setArcInductJumpMinThresh(float inductJumpMinThresh);
void setArcFftEnabled(char fftEnabled);
#endif
