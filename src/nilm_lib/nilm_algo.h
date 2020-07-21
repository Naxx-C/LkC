#ifndef __NILM_ALGO
#define __NILM_ALGO
#include "nilm_appliance.h"

typedef struct {

    int eventId;
    signed char action;
    signed char applianceId;
    float activePower; // powerline's realtime active power
    float voltage;
    float possiblity;
    int eventTime;
    float feature[8]; // as nilm feature
} NilmEvent;

static double featureMatch(float fv1[], float normalizedFv2[]);
static int getRatioLevel(float fv[]);
int nilmAnalyze(float current[], float voltage[], int length, int utcTime, float effCurrent, float effVoltage,
        float realPower, float oddFft[]);
NilmAppliance* createNilmAppliance(char name[], int nameLen, char id, float accumulatedPower);
#define ACTION_NONE  0
#define ACTION_ON  1
#define ACTION_OFF  -1


#endif
