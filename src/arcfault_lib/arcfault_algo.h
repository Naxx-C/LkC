#ifndef __ARCFAULT_ALGO
#define __ARCFAULT_ALGO
char arcAnalyze(int channel, float *current, int length, int *outArcNum, int *thisPeriodNum);
/**
 * @param current
 *            一个采样周期的电流数据。
 * @param length
 *            采样电流数据长度。当前默认为128
 * @param effCurrent
 *            电流有效值。为避免重复计算，有计算过则直接传入，未计算过则传入-1
 * @param oddFft
 *            1,3,5,7,9次奇次谐波数据。有计算过则直接传入，否则设为NULL
 * @param outArcNum
 *            output检测到的电弧数目(不一定是故障电弧，好弧也包含在内),可设置为NULL
 * @return 是否需要故障报警,0不需要,1需要,-1未初始化
 */
char arcAnalyzeInner(int channel, float *current, const int length, float effCurrent, float *oddFft,
        int *outArcNum, int *thisPeriodNum);
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
//初始化，通道数
void arcALgoInit(int channelNum);
/**
 * @return 返回4个字节版本号，第1个字节为完全重构；第2个字节为功能修改；第3、4个字节为bug修复等小改动
 */
int getArcAlgoVersion(void);
#endif
