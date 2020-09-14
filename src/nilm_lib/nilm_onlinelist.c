#include <string.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include "nilm_onlinelist.h"
#include "nilm_appliance.h"

static int gOnlineListCounter = 0;
static OnlineAppliance gOnlineList[ONLINELIST_MAX_NUM];

static MatchedAppliance gMatchedList[MATCHEDLIST_MAX_NUM];
static int gMatchedListCounter = 0;
static int gMatchedListWriteIndex = 0;
void addToMatchedList(MatchedAppliance *new) {

    if (gMatchedListWriteIndex >= MATCHEDLIST_MAX_NUM)
        gMatchedListWriteIndex = 0;

    float tmpMinPossibility = 1;
    int minPossibilityIndex = -1;
    char addedAlready = 0;
    for (int i = 0; i < gMatchedListCounter; i++) {
        MatchedAppliance *m = &(gMatchedList[i]);
        if (m->id == new->id) {
            if (m->possiblity < new->possiblity) { //同一电器,用概率更高的替代
                m->possiblity = new->possiblity;
                m->activePower = new->activePower;
            }
            addedAlready = 1;
        }

        if (m->possiblity < tmpMinPossibility) {
            tmpMinPossibility = m->possiblity;
            minPossibilityIndex = i;
        }
    }

    if (!addedAlready) {
        if (gMatchedListCounter < MATCHEDLIST_MAX_NUM) {
            memcpy(gMatchedList + gMatchedListWriteIndex++, new, sizeof(MatchedAppliance));
        } else {
            memcpy(gMatchedList + minPossibilityIndex, new, sizeof(MatchedAppliance));
        }
        if (gMatchedListCounter < MATCHEDLIST_MAX_NUM)
            gMatchedListCounter++;
    }
}

void getMatchedList(MatchedAppliance *matchedList, int *matchedListCounter) {
    matchedList = gMatchedList;
    *matchedListCounter = gMatchedListCounter;
}

void fillInWaitingCheckInfos(signed char *possibleIds, int *countdownTimer, int size, int utcTime) {

    int num = size < gMatchedListCounter ? size : gMatchedListCounter;

    for (int i = 0; i < num; i++) {
        MatchedAppliance *m = &(gMatchedList[i]);
        possibleIds[i] = m->id;
        ApplianceAdditionalInfo *info = getApplianceAdditionalInfo(m->id);
        if (info != NULL) {
            countdownTimer[i] = info->minUseTime;
        }
    }
}

//gMatchedList为临时数组,使用前恢复
void clearMatchedList() {
    gMatchedListCounter = 0;
    gMatchedListWriteIndex = 0;
}

void getBestMatchedApp(float deltaActivePower, signed char *bestMatchedId, float *possibility) {

    int mostPossIndex = -1;
    float maxPoss = 0;
    for (int i = 0; i < gMatchedListCounter; i++) {
        MatchedAppliance *m = &(gMatchedList[i]);
        if (m->possiblity > maxPoss) {
            maxPoss = m->possiblity;
            mostPossIndex = i;
        }
    }

    if (deltaActivePower < 0) {
        for (int i = 0; i < gMatchedListCounter; i++) {
            MatchedAppliance *m = &(gMatchedList[i]);
            if (m->id == gMatchedList[mostPossIndex].id)
                continue;
            //移除时，优先判断为已经在线电器
            if (isOnline(m->id) && !isOnline(gMatchedList[mostPossIndex].id)
                    && m->possiblity >= gMatchedList[mostPossIndex].possiblity * 0.7f) {
                mostPossIndex = i;
            }
        }
    } else {
        for (int i = 0; i < gMatchedListCounter; i++) {
            MatchedAppliance *m = &(gMatchedList[i]);
            if (m->id == gMatchedList[mostPossIndex].id)
                continue;
            //接入时，优先判断为已经在线电器
            if (isOnline(m->id) && !isOnline(gMatchedList[mostPossIndex].id)
                    && m->possiblity >= gMatchedList[mostPossIndex].possiblity * 0.95f) {
                mostPossIndex = i;
            }
        }
    }

    if (mostPossIndex >= 0) {
        *bestMatchedId = gMatchedList[mostPossIndex].id;
        *possibility = gMatchedList[mostPossIndex].possiblity;
    } else {
        *bestMatchedId = -1;
        *possibility = -0;
    }
}

/**
 *  new: 通过新事件触发
 */
void updateOnlineListByEvent(OnlineAppliance *new) {
    if (new->id <= NULL_APP_ID)
        return;

    if (isOnline(new->id)) {
        for (int i = 0; i < gOnlineListCounter; i++) {
            OnlineAppliance *o = &(gOnlineList[i]);
            if (o->id == new->id) {
                float previousPower = o->activePower;
                o->activePower += new->activePower;
                if (o->activePower > o->runningMaxActivePower) {
                    o->runningMaxActivePower = o->activePower;
                }

                if (o->activePower < 80 || o->activePower < previousPower * 0.1) {
                    //小功率移除
                    removeFromOnlineList(o->id);
                }
                break;
            }
        }

    } else {
        if (new->activePower < 0) {
            return;
        }

        new->runningMaxActivePower = new->activePower;
        //list已满,移除最老的1个
        if (gOnlineListCounter >= ONLINELIST_MAX_NUM) {
            for (int i = 0; i < gOnlineListCounter - 1; i++) {
                memcpy(gOnlineList + i, gOnlineList + i + 1, sizeof(OnlineAppliance));
            }
            memcpy(gOnlineList + gOnlineListCounter - 1, new, sizeof(OnlineAppliance));
        } else { //直接插入到list尾
            memcpy(gOnlineList + gOnlineListCounter, new, sizeof(OnlineAppliance));
            if (gOnlineListCounter < ONLINELIST_MAX_NUM)
                gOnlineListCounter++;
        }
    }
}

/**
 *  new: 直接修改在线电器
 */
void updateOnlineListDirectly(OnlineAppliance *new) {
    if (new->id <= NULL_APP_ID || new->activePower < 0)
        return;

    if (isOnline(new->id)) {
        for (int i = 0; i < gOnlineListCounter; i++) {
            OnlineAppliance *o = &(gOnlineList[i]);
            if (o->id == new->id) {
                new->runningMaxActivePower =
                        new->runningMaxActivePower > o->activePower ?
                                new->runningMaxActivePower : o->activePower;
                memcpy(o, new, sizeof(OnlineAppliance));
            }
        }
    } else {
        new->runningMaxActivePower = new->activePower;
        //list已满,移除最老的1个
        if (gOnlineListCounter >= ONLINELIST_MAX_NUM) {
            for (int i = 0; i < gOnlineListCounter - 1; i++) {
                memcpy(gOnlineList + i, gOnlineList + i + 1, sizeof(OnlineAppliance));
            }
            memcpy(gOnlineList + gOnlineListCounter - 1, new, sizeof(OnlineAppliance));
        } else { //直接插入到list尾
            memcpy(gOnlineList + gOnlineListCounter, new, sizeof(OnlineAppliance));
            if (gOnlineListCounter < ONLINELIST_MAX_NUM)
                gOnlineListCounter++;
        }
    }
}

/**
 *  更新在线列表里的功耗
 *  TODO: 时间是否会存在不稳定或跳变？
 *  totalPowerCost: 时间段内总功耗，单位kws
 *  lastUpdatedActivePower: 上一次更新功耗时的线路总的有功功率
 */
static int gLastPowercostUpdateTime = 0;
void updatePowercost(int utcTime, float *totalPowerCost, float lastUpdatedActivePower) {

    int timeDelta = utcTime - gLastPowercostUpdateTime;
    if (timeDelta > 0 && timeDelta <= 30) { //限定为[0,30s],防止时间错乱带来的重大影响

        *totalPowerCost += lastUpdatedActivePower / 1000 * (utcTime - gLastPowercostUpdateTime);
        for (int i = 0; i < gOnlineListCounter; i++) {
            OnlineAppliance *o = &(gOnlineList[i]);
//            o->powerCost += o->estimatedActivePower / 1000 * (utcTime - gLastPowercostUpdateTime);
            o->powerCost += o->activePower / 1000 * (utcTime - gLastPowercostUpdateTime);
        }
    }
    gLastPowercostUpdateTime = utcTime;
}

//清空在线列表
void clearOnlineList() {

    for (int i = 0; i < gOnlineListCounter; i++) {
        OnlineAppliance *o = &(gOnlineList[i]);
        handleApplianceOff(o);
    }
    gOnlineListCounter = 0;
}

char isOnline(signed char id) {

    if (id <= NULL_APP_ID) {
        return 0;
    }
    for (int i = 0; i < gOnlineListCounter; i++) {
        OnlineAppliance *o = &(gOnlineList[i]);
        if (o->id == id)
            return 1;
    }
    return 0;
}

char isOnlineByEventId(int eventId) {

    if (eventId <= 0) {
        return 0;
    }
    for (int i = 0; i < gOnlineListCounter; i++) {
        OnlineAppliance *o = &(gOnlineList[i]);
        if (o->eventId == eventId)
            return 1;
    }
    return 0;
}

extern void handleApplianceOff(OnlineAppliance *oa);
void removeFromOnlineList(signed char id) {

    for (int i = 0; i < gOnlineListCounter; i++) {
        OnlineAppliance *o = &(gOnlineList[i]);
        if (o->id == id) {
            handleApplianceOff(o);
            //list后续元素前移
            for (int j = i; j < gOnlineListCounter - 1; j++) {
                memcpy(gOnlineList + j, gOnlineList + j + 1, sizeof(OnlineAppliance));
            }
            gOnlineListCounter--;
            gOnlineList[gOnlineListCounter].id = NULL_APP_ID;
        }
    }
}

void getOnlineList(OnlineAppliance *onlineList, int *onlineListCounter) {

    onlineList = gOnlineList;
    *onlineListCounter = gOnlineListCounter;
}

//计算已识别电器的功率总和
float getOnlineListPower() {

    float power = 0;
    for (int i = 0; i < gOnlineListCounter; i++) {
        OnlineAppliance *o = &(gOnlineList[i]);
        power += o->activePower;
    }
    return power;
}

//异常处理
void powerCheck(float totalPower) {

    if (totalPower > 0) {
        float listPower = 0;
        float deltaPower = 0, tmpMin = 100000;
        signed char idMin = -1;
        while (totalPower < (listPower = getOnlineListPower()) * 0.9) {
            deltaPower = listPower - totalPower;

            for (int i = 0; i < gOnlineListCounter; i++) {
                OnlineAppliance *o = &(gOnlineList[i]);
                float delta = 0;
                if ((delta = fabs(o->activePower - deltaPower)) < tmpMin) {
                    idMin = o->id;
                    tmpMin = delta;
                }
            }
            if (idMin >= 0)
                removeFromOnlineList(idMin);
        }
    }
}

//根据实时电压调整功率
void updateOnlineAppPower(float voltage) {

    for (int i = 0; i < gOnlineListCounter; i++) {
        OnlineAppliance *o = &(gOnlineList[i]);
        if (o->id <= NULL_APP_ID)
            continue;
        o->activePower = voltage * voltage / o->voltage / o->voltage * o->activePower;
        o->voltage = voltage;
    }
}

