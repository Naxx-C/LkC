#ifndef __NILM_APPLIANCE
#define __NILM_APPLIANCE
#include "nilm_feature.h"
#define APP_NAME_LEN 16
#define APP_FEATURE_NUM 3
#define NULL_APP_ID -1

typedef struct {
    char name[APP_NAME_LEN];
    signed char id;
    float accumulatedPower;
    int featureCounter;
    int featureWriteIndex;
    NilmFeature nilmNormalizedFeatures[APP_FEATURE_NUM];

} NilmAppliance;


void addNormalizedFeature(NilmAppliance* na, float fv[]);
#endif
