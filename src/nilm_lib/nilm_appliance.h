#ifndef __NILM_APPLIANCE
#define __NILM_APPLIANCE
#include "nilm_feature.h"
#define APP_NAME_LEN 16
#define APP_FEATURE_NUM 5
//空id
#define NULL_APP_ID 0
#define BACKGROUND_UKNOWN LOAD_ID 0xFF

typedef struct {
    char name[APP_NAME_LEN];
    signed char id;
    float accumulatedPower;
    int featureCounter;
    int featureWriteIndex;
    NilmFeature nilmNormalizedFeatures[APP_FEATURE_NUM];

} NilmAppliance;

typedef struct {
    signed char id;
    float minPower; //最小功率
    float maxPower; //最大功率
    char maybeModeChange; //运行过程中是否可能发生模式切换
    int minUseTime; //单次最小使用时长，单位S
    int maxUseTime; //单次最大使用时长
    int useHourBitmapInDay; //每日使用时间段,24小时用bitmap表示
    int supportedEnv;
} ApplianceAdditionalInfo;

#define ENV_FULL  0xFF             //全开放
#define ENV_HOME (0x1 << 1)       //家庭环境(
#define  ENV_OFFICE (0x1 << 2)    //办公室环境
#define ENV_SIMPLE_TEST (0x1 << 3) //简易/临时搭建的测试环境

#define APPID_FIXFREQ_AIRCONDITIONER 0x01
#define APPID_VARFREQ_AIRCONDITIONER 0x02
#define APPID_WATER_HEATER 0x03
#define APPID_MICROWAVE_OVEN 0x07
#define APPID_ROLLER_WASHER 0x08
#define APPID_TURBO_WASHER 0x09
#define APPID_HAIRDRYER 0x0E
#define APPID_CLEANER 0x12

void addNormalizedFeature(NilmAppliance *na, float fv[]);
ApplianceAdditionalInfo* getApplianceAdditionalInfo(signed char id);

#endif
