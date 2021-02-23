#ifndef __ARCFAULT_ALGO
#define __ARCFAULT_ALGO
/**ATTENTION!!!
 * You must define APP_BUILD_DATE as following code in your project which includes this library
 * */
// const char *APP_BUILD_DATE = __DATE__;

int arcAnalyze(int channel, float *current, int length, int *outArcNum, int *thisPeriodNum);
/**
 * @param current
 *            current sample array
 * @param length
 *            current sample array length, default 128
 * @param effCurrent
 *            effective value. To avoid double counting, if you have get it, pass it in, otherwise set to -1;
 * @param oddFft
 *            1,3,5,7,9 harmonic wave, can be NULL
 * @param outArcNum
 *            arc nums within 1s(maybe not arc fault),can be NULL
 * @param thisPeriodNum
 *            arc nums in this period, can be NULL
 * @return if we need arc fault alarm now. 0 no, 1 yes, -1 not initialized correctly
 */
int arcAnalyzeInner(int channel, float *current, const int length, float effCurrent, float *oddFft,
        int *outArcNum, int *thisPeriodNum);
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
void setArcCurrentRange(float minCurrent, float maxCurrent);
int arcAlgoStatus(void);
#define ARC_CON_PREJ 1
#define ARC_CON_PN 2
#define ARC_CON_CONS 3
#define ARC_CON_PREC 4
#define ARC_CON_EXTR 5
#define ARC_CON_WIDT 6
#define ARC_CON_BJ 7
#define ARC_CON_POSJ 8
void setArcCheckDisabled(int item);
#define ARCFAULT_SENSITIVITY_LOW 0
#define ARCFAULT_SENSITIVITY_MEDIUM 1
#define ARCFAULT_SENSITIVITY_HIGH 2
void setArcfaultAlarmMode(int mode);
/**
 * @param channelNum
 *            channel num, must be <= 8
 * @return 0=normal <0=fatal error >0=something to be checked
 */
int arcAlgoInit(int channelNum);
/**
 * @return 4 bytes
 * byte 1 changed when completely restructured; byte 2 changed when some functions changed;
 * byte 3/4 changed when there are some small changes or some bug fixes
 */
int getArcAlgoVersion(void);
#endif
