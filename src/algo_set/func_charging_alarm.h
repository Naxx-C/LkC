#ifndef FUNC_CHARGING_ALARM_ALGO_
#define FUNC_CHARGING_ALARM_ALGO_
#include "algo_base_struct.h"
/**
 * volStart:序列起始位置.要保证从起始位置后至少有128个采样点
 * fft:谐波数组,大小为5,包含一三五七九次谐波分量
 * pulseI:启动冲量
 * deltaActivePower:差分有功功率
 * errMsg:错误信息输出,如不需要可以置NULL
 */
int chargingDetect(int channel, float *fft, float pulseI, float curSamplePulse, float deltaActivePower,
        float deltaReactivePower, int startupTime, WaveFeature *wf, char *errMsg);

#define CHARGING_ALARM_SENSITIVITY_LOW 0
#define CHARGING_ALARM_SENSITIVITY_MEDIUM 1
#define CHARGING_ALARM_SENSITIVITY_HIGH 2
void setChargingAlarmSensitivity(int channel, int mode);
int getChargingAlarmSensitivity(int channel);

//init ok: return 0
int initFuncChargingAlarm(void);
#endif
