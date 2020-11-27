/*
 * Author: TPSON
 * Release to CHTQDQ
 * 2020-11-20
 */
#ifndef ALGO_SET_H_
#define ALGO_SET_H_
//const char *APP_BUILD_DATE = __DATE__;
/*
 * Feed original current and voltage sampling data to the algo modual
 * return error num. 0 is ok.
 * cur: realtime current sampling with 6.4k
 * vol: realtime current sampling with 6.4k
 * unixTimestamp: unixTimestamp like 1605098030 (2020/11/11 20:33:50)
 * extraMsg: some output info like error msg.Can be NULL
 */
int feedData(float *cur, float *vol, int unixTimestamp, char *extraMsg);
int initTpsonAlgoLib(void);

#define ALGO_CHARGING_DETECT 0
#define ALGO_DORM_CONVERTER_DETECT 1
#define ALGO_MALICIOUS_LOAD_DETECT 2
#define ALGO_ARCFAULT_DETECT       3
int setModuleEnable(int module, int enable);
int getChargingDetectResult(void);
int getDormConverterDetectResult(void);
int getMaliLoadDetectResult(void);
int getArcfaultDetectResult(int *arcNum, int *onePeriodNum);

//power cost since device reboot - kws
int getPowerCost(void);

void setMinEventDeltaPower(float minEventDeltaPower);
/**charging alarm config api*/
#define CHARGING_ALARM_SENSITIVITY_LOW 0
#define CHARGING_ALARM_SENSITIVITY_MEDIUM 1
#define CHARGING_ALARM_SENSITIVITY_HIGH 2
void setChargingAlarmMode(int mode); //default MEDIUM
void setMinChargingDevicePower(float power);
void setMaxChargingDevicePower(float power);

/**dorm converter detection config api*/
#define DORM_CONVERTER_SENSITIVITY_LOW 0
#define DORM_CONVERTER_SENSITIVITY_MEDIUM 1
#define DORM_CONVERTER_SENSITIVITY_HIGH 2
void setDormConverterAlarmMode(int mode); //default MEDIUM
void setMinDormConverterPower(float power);//default 150w
void setMaxDormConverterPower(float power);

/**malicious load detection config api*/
#define MALI_LOAD_SENSITIVITY_LOW 0
#define MALI_LOAD_SENSITIVITY_MEDIUM 1
#define MALI_LOAD_SENSITIVITY_HIGH 2
void setMaliLoadAlarmMode(int mode);
void addMaliciousLoadWhitelist(float power);
void setMaliLoadWhitelistMatchRatio(float minRatio, float maxRatio); //default min=0.9,max=1.1
void setMaliLoadMinPower(float minPower); //default 200w
#endif /* ALGO_SET_ALGO_SET_H_ */
