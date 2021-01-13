#ifndef FUNC_ARCFAULT_
#define FUNC_ARCFAULT_

void setArcMinExtremeDis(int minExtremeDis);
void setArcMinWidth(int minWidth);
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
void setArcOverlayCheckEnabled(char enable);
int arcAlgoStatus(void);
int arcfaultDetect(int channel, float *current, float effValue, float *oddFft, int *outArcNum,
        int *outThisPeriodNum, char *msg);
#define ARC_CON_PREJ 1
#define ARC_CON_PN 2
#define ARC_CON_CONS 3
#define ARC_CON_PREC 4
#define ARC_CON_EXTR 5
#define ARC_CON_WIDT 6
#define ARC_CON_BJ 7
#define ARC_CON_POSJ 8
void setArcCheckDisabled(int item);
/**
 * @return 0=normal <0=fatal error >0=something to be checked
 */
#define ARCFAULT_SENSITIVITY_LOW 0
#define ARCFAULT_SENSITIVITY_MEDIUM 1
#define ARCFAULT_SENSITIVITY_HIGH 2
void setArcfaultAlarmMode(int channel, int mode);

int initFuncArcfault(void);

#endif
