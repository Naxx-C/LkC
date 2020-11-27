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
    signed char possibleIds[MATCHEDLIST_MAX_NUM]; //候选待确认id
    int delayedTimer[MATCHEDLIST_MAX_NUM]; //待确认倒计时
} NilmEvent;

typedef struct {

    float activePower; // powerline's total power, adjusted by voltage
    float reactivePower;
    float totalPowerCost;
    int sampleTime; // in second
} __attribute__ ((packed)) NilmPowerShaft;

typedef struct {
    int flipTimes;
    char haveNonPureResLoad; //是否有非纯阻性事件发生
    char haveSteplessChange; //出现缓慢变化
} FootprintResult;

typedef struct {
    int flatNum; //平肩数目
    int extremeNum; //极值点数目
    float activePower; //有功功率
    float reactivePower; //无功功率
} StableFeature;

double featureMatch(float fv1[], float normalizedFv2[]);
int getRatioLevel(float fv[]);
int nilmAnalyze(float current[], float voltage[], int length, int utcTime, float effCurrent, float effVoltage,
        float realPower, float oddFft[]);
NilmAppliance* createNilmAppliance(char name[], int nameLen, char id, float accumulatedPower);
int getNilmAlgoVersion();
void setNilmWorkEnv(int env);
int nilmInit();
int getNilmWorkEnv();
void setNilmMinEventStep(int minEventStep);
void setInitWaitingCheckStatus(OnlineAppliance *oa);
void checkWaitingNilmEvents(int currentTime);
void updateOnlineAppPower(signed char id, float activePower);

#define ACTION_NONE  0
#define ACTION_ON  1
#define ACTION_OFF  -1

#endif
