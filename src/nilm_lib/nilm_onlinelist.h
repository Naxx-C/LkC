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
    signed char isWaitingForCheck; //是否待确定
    float activePower;//实际差分有功
    float voltage;
    float possiblity;
    int poweronTime;
    int eventId; //对应事件的id
    float estimatedActivePower; //综合判断的估计电量,对比变频设备一般会大于实际差分有功
    float powerCost; //kws，需/3600转为kwh
} OnlineAppliance;

#define MATCHEDLIST_MAX_NUM 5
typedef struct {

    signed char id;
    float activePower;
    float possiblity;
} MatchedAppliance;

void addToMatchedList(MatchedAppliance *new);

//gMatchedList为临时数组,使用前恢复
void clearMatchedList();

void getBestMatchedApp(float deltaActivePower, signed char *bestMatchedId,float* possibility);

void updateOnlineList(OnlineAppliance *new);
void updatePowercost(int utcTime, float *totalPowerCost, float lastUpdatedActivePower);
void abnormalCheck(float totalPower);

//gMatchedList为临时数组,使用前恢复
void clearOnlineList();
void removeFromOnlineList(signed char id);
char isOnline(signed char id);

void getOnlineList(OnlineAppliance *onlineList, int *size);
float getOnlineListPower();
void updateOnlineAppPower(float voltage);

#endif
