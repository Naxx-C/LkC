/*
 * Author: TPSON
 * 2020-11-20
 * 注：所有的配置均配置到内存,断电失效。如需要断电保存，需要业务层自行进行flash save/load操作
 */
#ifndef ALGO_SET_H_
#define ALGO_SET_H_
//const char *APP_BUILD_DATE = __DATE__;
/*
 * 输入电流电压采样数据到算法计算模块
 * 返回值0表示正确
 * cur: 6.4k/s实时电流采样数据
 * vol: 6.4k/s实时电压采样数据
 * unixTimestamp: unix时间戳,如 1605098030 (2020/11/11 20:33:50)
 * extraMsg: 算法模块对外输出信息,可设置为NULL
 */
int feedData(int channel, float *cur, float *vol, int unixTimestamp, char *extraMsg);
//算法模块初始化
int initTpsonAlgoLib(void);

#define ALGO_CHARGING_DETECT 0
#define ALGO_DORM_CONVERTER_DETECT 1
#define ALGO_MALICIOUS_LOAD_DETECT 2
#define ALGO_ARCFAULT_DETECT       3
//算法子功能使能,默认全部关闭
int setModuleEnable(int module, int enable);
//获取对应功能返回值,返回1为报警
int getChargingDetectResult(int channel);
int getDormConverterDetectResult(int channel);
int getMaliLoadDetectResult(int channel);
/*
 * arcNum: 1S内故障电弧个数
 * onePeriodNum: 当前采样周期内故障电弧个数
 */
int getArcfaultDetectResult(int channel, int *arcNum, int *onePeriodNum);
//从开机以来累计功耗, 单位kws(kwh需要除以3600转换)
float getPowerCost(int channel);
//设置事件检测最小功率变化值,小于此功率的变化将被忽略，默认90w
void setMinEventDeltaPower(int channel, float minEventDeltaPower);


/**charging alarm config api*/
#define CHARGING_ALARM_SENSITIVITY_LOW 0
#define CHARGING_ALARM_SENSITIVITY_MEDIUM 1
#define CHARGING_ALARM_SENSITIVITY_HIGH 2
//设置充电检测的灵敏度, 默认MEDIUM
void setChargingAlarmSensitivity(int channel, int mode);
//设置最小覆盖功率,默认90w
void setMinChargingDevicePower(int channel, float power);
//设置最大覆盖功率,默认280w
void setMaxChargingDevicePower(int channel, float power);


/**dorm converter detection config api*/
#define DORM_CONVERTER_SENSITIVITY_LOW 0
#define DORM_CONVERTER_SENSITIVITY_MEDIUM 1
#define DORM_CONVERTER_SENSITIVITY_HIGH 2
//设置宿舍调压器检测的灵敏度, 默认MEDIUM
void setDormConverterAlarmSensitivity(int channel, int mode);
//设置最小覆盖功率,默认150w
void setMinDormConverterPower(int channel, float power);
//设置最大覆盖功率,默认3000w
void setMaxDormConverterPower(int channel, float power);


/**malicious load detection config api*/
#define MALI_LOAD_SENSITIVITY_LOW 0
#define MALI_LOAD_SENSITIVITY_MEDIUM 1
#define MALI_LOAD_SENSITIVITY_HIGH 2
//设置宿舍恶性负载检测的灵敏度, 默认MEDIUM
void setMaliLoadAlarmSensitivity(int channel, int mode);
/*
 * 添加白名单电器,如宿舍常用的饮水机
 * power: 白名单电器的功率
 */
void addMaliciousLoadWhitelist(int channel, float power);
/*
 * 将电器从白名单删除(自动寻找最匹配的删除,不需要严格一致)
 * power: 电器的功率
 */
int removeFromMaliLoadWhitelist(int channel, float power); //remove the best matched power
//设置白名单电器功率误差范围,默认min=0.9,max=1.1
void setMaliLoadWhitelistMatchRatio(int channel, float minRatio, float maxRatio);
//设置恶性负载最小检测范围,默认200w
void setMaliLoadMinPower(int channel, float minPower);


/**arcfault detection config api*/
#define ARCFAULT_SENSITIVITY_LOW 0
#define ARCFAULT_SENSITIVITY_MEDIUM 1
#define ARCFAULT_SENSITIVITY_HIGH 2
//设置故障电弧检测的灵敏度, 默认MEDIUM
void setArcfaultSensitivity(int channel, int sensitivity);
#define ARCFAULT_SMARTMODE_OFF 0
#define ARCFAULT_SMARTMODE_ON 1
//是否使能智能模式, 默认ON. 如演示等非正常环境使用时,建议临时关闭
int setArcfaultSmartMode(int channel, int mode);
//设置环境适应性学习时间，单位s(默认3天). 学习期间将暂时屏蔽故障电弧报警
void setArcLearningTime(int channel, int learningTime);
//启动环境适应性学习，将会在所配置的学习时间到时，自动停止学习。
void startArcLearning(int channel);
//手动停止环境适应性学习
void stopArcLearning(int channel);
//清除本机所有学习的结果(不分通道)
void clearArcLearningResult(void);
//其他非常用配置
void setArcMinWidth(int minWidth);
void setArcCurrentRange(float minCurrent, float maxCurrent);//默认[1.5A-100A]

#endif /* ALGO_SET_ALGO_SET_H_ */
