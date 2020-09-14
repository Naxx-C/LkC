#ifndef __NILM_ALGO
#define __NILM_ALGO
#include "nilm_appliance.h"
#include "nilm_onlinelist.h"

typedef struct {

    int eventId;
    signed char applianceId;
    float activePower; // powerline's realtime active power
    float voltage;
    float possiblity;
    int eventTime;
    float feature[8]; // as nilm feature
    signed char possibleIds[MATCHEDLIST_MAX_NUM];//候选待确认id
    int delayedTimer[MATCHEDLIST_MAX_NUM]; //待确认倒计时
} NilmEvent;

typedef struct {

    float deltaPower; // delta active power
    float deltaPowerFactor;
    float linePower; // powerline's total power
    float voltage;
    int eventTime; // in second
} __attribute__ ((packed)) NilmEventFootprint;

typedef struct {
    int flipTimes;
} FootprintResult;

//typedef struct {
//
//    float deltaPower; // delta active power
//    float deltaPowerFactor;
//    float linePower; // powerline's total power
//    float voltage;
//    int eventTime; // in second
//} __attribute__ ((packed)) NilmEventFootprint;

double featureMatch(float fv1[], float normalizedFv2[]);
int getRatioLevel(float fv[]);
int nilmAnalyze(float current[], float voltage[], int length, int utcTime, float effCurrent, float effVoltage,
        float realPower, float oddFft[]);
NilmAppliance* createNilmAppliance(char name[], int nameLen, char id, float accumulatedPower);
int getNilmAlgoVersion();
void setNilmWorkEnv(int env);
int getNilmWorkEnv();
void setNilmMinEventStep(int minEventStep);
void setInitWaitingCheckStatus(OnlineAppliance *oa);
void checkWaitingNilmEvents(int currentTime);

#define ACTION_NONE  0
#define ACTION_ON  1
#define ACTION_OFF  -1

#endif
