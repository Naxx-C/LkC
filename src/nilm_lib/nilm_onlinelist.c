#include <string.h>
#include <stddef.h>
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

//gMatchedList为临时数组,使用前恢复
void clearMatchedList() {
    gMatchedListCounter = 0;
    gMatchedListWriteIndex = 0;
}

void getBestMatchedOnline(float deltaActivePower, signed char *bestMatchedId, float *possibility) {

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
            //移除时，优先判断为已经在线电器
            if (isOnline(m->id)) {
                if (isOnline(gMatchedList[mostPossIndex].id)
                        && m->possiblity > gMatchedList[mostPossIndex].possiblity) {
                    mostPossIndex = i;

                } else if (!isOnline(gMatchedList[mostPossIndex].id)
                        && m->possiblity >= gMatchedList[mostPossIndex].possiblity * 0.7f) {
                    mostPossIndex = i;
                }
            }
        }

    } else {
        for (int i = 0; i < gMatchedListCounter; i++) {
            MatchedAppliance *m = &(gMatchedList[i]);
            //接入时，优先判断为已经在线电器
            if (isOnline(m->id)) {
                if (isOnline(gMatchedList[mostPossIndex].id)
                        && m->possiblity > gMatchedList[mostPossIndex].possiblity) {
                    mostPossIndex = i;

                } else if (!isOnline(gMatchedList[mostPossIndex].id)
                        && m->possiblity >= gMatchedList[mostPossIndex].possiblity * 0.95f) {
                    mostPossIndex = i;
                }
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

void addToOnlineList(OnlineAppliance *new) {

    if (isOnline(new->id)) {
        for (int i = 0; i < gOnlineListCounter; i++) {
            OnlineAppliance *o = &(gOnlineList[i]);
            if (o->id == new->id) {
                o->activePower += new->activePower;
                if (o->activePower < 80) {
                    removeFromOnlineList(o->id);
                }
                break;
            }
        }

    } else {

        //list已满,移除时间最老的1个
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

//gMatchedList为临时数组,使用前恢复
void clearOnlineList() {
    gOnlineListCounter = 0;
}

char isOnline(signed char id) {

    for (int i = 0; i < gOnlineListCounter; i++) {
        OnlineAppliance *o = &(gOnlineList[i]);
        if (o->id == id)
            return 1;
    }
    return 0;
}

void removeFromOnlineList(signed char id) {

    for (int i = 0; i < gOnlineListCounter; i++) {
        OnlineAppliance *o = &(gOnlineList[i]);
        if (o->id == id) {
            //list后续元素前移
            for (int j = i; j < gOnlineListCounter - 1; j++) {
                memcpy(gOnlineList + j, gOnlineList + j + 1, sizeof(OnlineAppliance));
            }
            gOnlineListCounter--;
            gOnlineList[gOnlineListCounter].id = NULL_APP_ID;
        }
    }
}

void getOnlineList(OnlineAppliance *onlineList, int *size) {

    onlineList = gOnlineList;
    *size = gOnlineListCounter;
}

float getOnlineListPower() {

    float power = 0;
    for (int i = 0; i < gOnlineListCounter; i++) {
        OnlineAppliance *o = &(gOnlineList[i]);
        power += o->activePower;
    }
    return power;
}

void updateOnlineAppPower(float voltage) {

    for (int i = 0; i < gOnlineListCounter; i++) {
        OnlineAppliance *o = &(gOnlineList[i]);
        o->activePower = voltage * voltage / o->voltage / o->voltage * o->activePower;
        o->voltage = voltage;
    }
}

