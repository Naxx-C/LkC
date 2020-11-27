#ifndef __NILM_ONLINELIST
#define __NILM_ONLINELIST
#include "nilm_appliance.h"

#define ONLINELIST_MAX_NUM 10
#define CATEGORY_HEATING 1
#define CATEGORY_UNBELIEVABLE 0
#define CATEGORY_UNKNOWN -1
typedef struct {

    signed char id;
    signed char category; //大类
    signed char isConfirmed; //是否待确定
    float activePower;//实际差分有功
    float voltage;
    float possiblity;
    int poweronTime;
    int eventId; //对应事件的id
    float estimatedActivePower; //综合判断的估计电量,对比变频设备一般会大于实际差分有功
    float runningMaxActivePower; //自动赋值,不需要手动赋值
    float totalPowerCostAudit; //启动时刻的当前线路总功耗记录
    float powerCost; //kws，需/3600转为kwh
} OnlineAppliance;

#define MATCHEDLIST_MAX_NUM 5
typedef struct {

    signed char id;
    float activePower;
    float possiblity;
} MatchedAppliance;

void addToMatchedList(MatchedAppliance *new);
void modifyLowPowerId(int dstId, float powerHighLimit);

//gMatchedList为临时数组,使用前恢复
void clearMatchedList();

void getBestMatchedApp(float deltaActivePower, signed char *bestMatchedId,float* possibility);
void getMatchedList(MatchedAppliance **matchedList, int *matchedListCounter);
char isInMatchedList(signed char id);

void updateOnlineListByEvent(OnlineAppliance *new);
void updatePowercost(int utcTime, float *totalPowerCost, float lastUpdatedActivePower);
void powerCheck(float totalPower);

//gMatchedList为临时数组,使用前恢复
void clearOnlineList();
void removeFromOnlineList(signed char id);
char isOnline(signed char id);
char isOnlineByEventId(int eventId);

void getOnlineList(OnlineAppliance **onlineList, int *size);
float getOnlineListPower();
void updateOnlineListPowerByVol(float voltage);
void adjustAirConditionerPower(float totalPower);

#endif
