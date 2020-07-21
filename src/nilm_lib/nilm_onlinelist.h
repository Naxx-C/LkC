#ifndef __NILM_ONLINELIST
#define __NILM_ONLINELIST
#include "nilm_appliance.h"

#define ONLINELIST_MAX_NUM 10
typedef struct {

    signed char id;
    float activePower;
    float voltage;
    float possiblity;
    int poweronTime;
} OnlineAppliance;

#define MATCHEDLIST_MAX_NUM 2
typedef struct {

    signed char id;
    float activePower;
    float possiblity;
} MatchedAppliance;

void addToMatchedList(MatchedAppliance *new);

//gMatchedList为临时数组,使用前恢复
void clearMatchedList();

void getBestMatchedOnline(float deltaActivePower, signed char *bestMatchedId,float* possibility);

void addToOnlineList(OnlineAppliance *new);

//gMatchedList为临时数组,使用前恢复
void clearOnlineList();
void removeFromOnlineList(signed char id);
char isOnline(signed char id);

void getOnlineList(OnlineAppliance *onlineList, int *size);
float getOnlineListPower();
void updateOnlineAppPower(float voltage);

#endif
