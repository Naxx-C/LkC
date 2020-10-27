#ifndef __CHARGING_ALARM_ALGO
#define __CHARGING_ALARM_ALGO
/**ATTENTION!!!
 * You must define APP_BUILD_DATE somewhere as following code in your project
 * */
// const char *APP_BUILD_DATE = __DATE__;
/**
 * cur:电流采样序列
 * curStart:序列起始位置.要保证从起始位置后至少有128个采样点
 * vol:电压采样序列
 * volStart:序列起始位置.要保证从起始位置后至少有128个采样点
 * fft:谐波数组,大小为5,包含一三五七九次谐波分量
 * pulseI:启动冲量
 * deltaEffI:差分有效电流
 * deltaEffU:差分时有效电压(等于当前电压)
 * deltaActivePower:差分有功功率
 * errMsg:错误信息输出,如不需要可以置NULL
 */
int isChargingDevice(float *cur, int curStart, float *vol, int volStart, float *fft, float pulseI,
        float deltaEffI, float deltaEffU, float deltaActivePower, char* errMsg);

#define CHARGING_ALARM_DISABLED -1
#define CHARGING_ALARM_SENSITIVITY_LOW 0
#define CHARGING_ALARM_SENSITIVITY_MEDIUM 1
#define CHARGING_ALARM_SENSITIVITY_HIGH 2
void setMode(int mode);
int getMode(void);
int getChargingAlarmAlgoVersion(void);

//init ok: return 0
int chargingAlarmAlgoinit(void);
#endif
