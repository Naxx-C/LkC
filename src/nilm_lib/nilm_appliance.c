#include "nilm_feature.h"
#include "nilm_appliance.h"
#include <string.h>

void addNormalizedFeature(NilmAppliance *na, float fv[]) {

    if (na->featureWriteIndex >= APP_FEATURE_NUM)
        na->featureWriteIndex = 0;
    NilmFeature *curFeature = &(na->nilmNormalizedFeatures[na->featureWriteIndex]);

    for (int i = 0; i < 5; i++) {
        curFeature->oddFft[i] = fv[i];
    }
    curFeature->powerFactor = normalizePowerFactor(fv[5]);
    curFeature->pulseI = normalizePulseI(fv[6]);
    curFeature->activePower = normalizeActivePower(fv[7]);
    na->featureWriteIndex++;
    if (na->featureCounter < APP_FEATURE_NUM) {
        na->featureCounter++;
    }
}

//typedef struct {
//    signed char id;
//    float minPower; //最小功率
//    float maxPower; //最大功率
//    char maybeModeChange; //运行过程中是否可能发生模式切换
//    int minUseTime; //单次最小使用时长，单位S
//    int maxUseTime; //单次最大使用时长
//    int useHourBitmapInDay; //每日使用时间段,24小时用bitmap表示
//    int supportedEnv;
//}
#define APPLIANCE_ADD_INFO_SIZE 30
ApplianceAdditionalInfo gApplianceAdditionalInfos[APPLIANCE_ADD_INFO_SIZE];
void nilmApplianceInit() {

    memset(gApplianceAdditionalInfos, 0, sizeof(gApplianceAdditionalInfos));
    //定频空调
    gApplianceAdditionalInfos[0].id = 0x01;
    gApplianceAdditionalInfos[0].supportedEnv = ENV_HOME;
    gApplianceAdditionalInfos[0].minPower = 600;
    gApplianceAdditionalInfos[0].maxPower = 1600;
    gApplianceAdditionalInfos[0].maybeModeChange = 0;

    //吹风机
    gApplianceAdditionalInfos[1].id = 0x0E;
    gApplianceAdditionalInfos[1].supportedEnv = ENV_FULL;
    gApplianceAdditionalInfos[1].minPower = 500;
    gApplianceAdditionalInfos[1].maxPower = 2300;
    gApplianceAdditionalInfos[1].maybeModeChange = 1;

}

ApplianceAdditionalInfo* getApplianceAdditionalInfo(signed char id) {

    for (int i = 0; i < APPLIANCE_ADD_INFO_SIZE; i++) {
        if(id==gApplianceAdditionalInfos[i].id)
            return &(gApplianceAdditionalInfos[i]);
    }
    return NULL;
}

