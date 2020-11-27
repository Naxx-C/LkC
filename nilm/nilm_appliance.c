#include "../nilm/nilm_appliance.h"

#include <string.h>
#include "../nilm/nilm_feature.h"

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

#define APPLIANCE_ADD_INFO_SIZE 30
ApplianceAdditionalInfo gApplianceAdditionalInfos[APPLIANCE_ADD_INFO_SIZE];
//TODO:待补充
void nilmApplianceInit() {

    memset(gApplianceAdditionalInfos, 0, sizeof(gApplianceAdditionalInfos));
    //定频空调
    gApplianceAdditionalInfos[0].id = 1;
    gApplianceAdditionalInfos[0].minUseTime = 1;
    gApplianceAdditionalInfos[0].maxUseTime = 9999;
    gApplianceAdditionalInfos[0].useHourBitmapInDay = 16777215;
    gApplianceAdditionalInfos[0].useMonthBitmapInYear = 6118;
    gApplianceAdditionalInfos[0].maybeModeChange = 1;
    gApplianceAdditionalInfos[0].minPower = 600.0;
    gApplianceAdditionalInfos[0].maxPower = 1600.0;
    gApplianceAdditionalInfos[0].supportedEnv = 255;
    //变频空调
    gApplianceAdditionalInfos[1].id = 2;
    gApplianceAdditionalInfos[1].minUseTime = 1;
    gApplianceAdditionalInfos[1].maxUseTime = 9999;
    gApplianceAdditionalInfos[1].useHourBitmapInDay = 16777215;
    gApplianceAdditionalInfos[1].useMonthBitmapInYear = 6118;
    gApplianceAdditionalInfos[1].maybeModeChange = 1;
    gApplianceAdditionalInfos[1].minPower = 600.0;
    gApplianceAdditionalInfos[1].maxPower = 2500.0;
    gApplianceAdditionalInfos[1].supportedEnv = 255;
    //热水器
    gApplianceAdditionalInfos[2].id = 3;
    gApplianceAdditionalInfos[2].minUseTime = 10;
    gApplianceAdditionalInfos[2].maxUseTime = 60;
    gApplianceAdditionalInfos[2].useHourBitmapInDay = 16777215;
    gApplianceAdditionalInfos[2].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[2].maybeModeChange = 0;
    gApplianceAdditionalInfos[2].minPower = 800.0;
    gApplianceAdditionalInfos[2].maxPower = 3000.0;
    gApplianceAdditionalInfos[2].supportedEnv = 1;
    //热水壶
    gApplianceAdditionalInfos[3].id = 4;
    gApplianceAdditionalInfos[3].minUseTime = 4;
    gApplianceAdditionalInfos[3].maxUseTime = 12;
    gApplianceAdditionalInfos[3].useHourBitmapInDay = 16777152;
    gApplianceAdditionalInfos[3].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[3].maybeModeChange = 0;
    gApplianceAdditionalInfos[3].minPower = 500.0;
    gApplianceAdditionalInfos[3].maxPower = 2200.0;
    gApplianceAdditionalInfos[3].supportedEnv = 255;
    //电饭煲
    gApplianceAdditionalInfos[4].id = 5;
    gApplianceAdditionalInfos[4].minUseTime = 20;
    gApplianceAdditionalInfos[4].maxUseTime = 150;
    gApplianceAdditionalInfos[4].useHourBitmapInDay = 2039744;
    gApplianceAdditionalInfos[4].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[4].maybeModeChange = 0;
    gApplianceAdditionalInfos[4].minPower = 500.0;
    gApplianceAdditionalInfos[4].maxPower = 1200.0;
    gApplianceAdditionalInfos[4].supportedEnv = 1;
    //电磁炉
    gApplianceAdditionalInfos[5].id = 6;
    gApplianceAdditionalInfos[5].minUseTime = 10;
    gApplianceAdditionalInfos[5].maxUseTime = 120;
    gApplianceAdditionalInfos[5].useHourBitmapInDay = 1981440;
    gApplianceAdditionalInfos[5].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[5].maybeModeChange = 0;
    gApplianceAdditionalInfos[5].minPower = 800.0;
    gApplianceAdditionalInfos[5].maxPower = 2200.0;
    gApplianceAdditionalInfos[5].supportedEnv = 1;
    //微波炉
    gApplianceAdditionalInfos[6].id = 7;
    gApplianceAdditionalInfos[6].minUseTime = 1;
    gApplianceAdditionalInfos[6].maxUseTime = 9999;
    gApplianceAdditionalInfos[6].useHourBitmapInDay = 4079552;
    gApplianceAdditionalInfos[6].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[6].maybeModeChange = 0;
    gApplianceAdditionalInfos[6].minPower = 800.0;
    gApplianceAdditionalInfos[6].maxPower = 1500.0;
    gApplianceAdditionalInfos[6].supportedEnv = 255;
    //滚筒洗衣机
    gApplianceAdditionalInfos[7].id = 8;
    gApplianceAdditionalInfos[7].minUseTime = 15;
    gApplianceAdditionalInfos[7].maxUseTime = 150;
    gApplianceAdditionalInfos[7].useHourBitmapInDay = 8128448;
    gApplianceAdditionalInfos[7].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[7].maybeModeChange = 1;
    gApplianceAdditionalInfos[7].minPower = 300.0;
    gApplianceAdditionalInfos[7].maxPower = 1100.0;
    gApplianceAdditionalInfos[7].supportedEnv = 1;
    //涡轮洗衣机
    gApplianceAdditionalInfos[8].id = 9;
    gApplianceAdditionalInfos[8].minUseTime = 15;
    gApplianceAdditionalInfos[8].maxUseTime = 150;
    gApplianceAdditionalInfos[8].useHourBitmapInDay = 8128448;
    gApplianceAdditionalInfos[8].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[8].maybeModeChange = 1;
    gApplianceAdditionalInfos[8].minPower = 200.0;
    gApplianceAdditionalInfos[8].maxPower = 500.0;
    gApplianceAdditionalInfos[8].supportedEnv = 1;
    //电视机
    gApplianceAdditionalInfos[9].id = 10;
    gApplianceAdditionalInfos[9].minUseTime = 3;
    gApplianceAdditionalInfos[9].maxUseTime = 360;
    gApplianceAdditionalInfos[9].useHourBitmapInDay = 16776960;
    gApplianceAdditionalInfos[9].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[9].maybeModeChange = 0;
    gApplianceAdditionalInfos[9].minPower = 70.0;
    gApplianceAdditionalInfos[9].maxPower = 350.0;
    gApplianceAdditionalInfos[9].supportedEnv = 255;
    //电冰箱
    gApplianceAdditionalInfos[10].id = 11;
    gApplianceAdditionalInfos[10].minUseTime = 4;
    gApplianceAdditionalInfos[10].maxUseTime = 30;
    gApplianceAdditionalInfos[10].useHourBitmapInDay = 16777215;
    gApplianceAdditionalInfos[10].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[10].maybeModeChange = 0;
    gApplianceAdditionalInfos[10].minPower = 70.0;
    gApplianceAdditionalInfos[10].maxPower = 650.0;
    gApplianceAdditionalInfos[10].supportedEnv = 255;
    //电烤箱
    gApplianceAdditionalInfos[11].id = 12;
    gApplianceAdditionalInfos[11].minUseTime = 3;
    gApplianceAdditionalInfos[11].maxUseTime = 120;
    gApplianceAdditionalInfos[11].useHourBitmapInDay = 4194176;
    gApplianceAdditionalInfos[11].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[11].maybeModeChange = 0;
    gApplianceAdditionalInfos[11].minPower = 900.0;
    gApplianceAdditionalInfos[11].maxPower = 3200.0;
    gApplianceAdditionalInfos[11].supportedEnv = 1;
    //油烟机
    gApplianceAdditionalInfos[12].id = 13;
    gApplianceAdditionalInfos[12].minUseTime = 5;
    gApplianceAdditionalInfos[12].maxUseTime = 120;
    gApplianceAdditionalInfos[12].useHourBitmapInDay = 2039680;
    gApplianceAdditionalInfos[12].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[12].maybeModeChange = 0;
    gApplianceAdditionalInfos[12].minPower = 140.0;
    gApplianceAdditionalInfos[12].maxPower = 350.0;
    gApplianceAdditionalInfos[12].supportedEnv = 1;
    //吹风机
    gApplianceAdditionalInfos[13].id = 14;
    gApplianceAdditionalInfos[13].minUseTime = 0;
    gApplianceAdditionalInfos[13].maxUseTime = 10;
    gApplianceAdditionalInfos[13].useHourBitmapInDay = 16254913;
    gApplianceAdditionalInfos[13].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[13].maybeModeChange = 1;
    gApplianceAdditionalInfos[13].minPower = 450.0;
    gApplianceAdditionalInfos[13].maxPower = 2200.0;
    gApplianceAdditionalInfos[13].supportedEnv = 255;
    //电暖器
    gApplianceAdditionalInfos[14].id = 15;
    gApplianceAdditionalInfos[14].minUseTime = 0;
    gApplianceAdditionalInfos[14].maxUseTime = 600;
    gApplianceAdditionalInfos[14].useHourBitmapInDay = 16777215;
    gApplianceAdditionalInfos[14].useMonthBitmapInYear = 6150;
    gApplianceAdditionalInfos[14].maybeModeChange = 0;
    gApplianceAdditionalInfos[14].minPower = 800.0;
    gApplianceAdditionalInfos[14].maxPower = 2200.0;
    gApplianceAdditionalInfos[14].supportedEnv = 255;
    //饮水机
    gApplianceAdditionalInfos[15].id = 16;
    gApplianceAdditionalInfos[15].minUseTime = 3;
    gApplianceAdditionalInfos[15].maxUseTime = 20;
    gApplianceAdditionalInfos[15].useHourBitmapInDay = 16777215;
    gApplianceAdditionalInfos[15].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[15].maybeModeChange = 0;
    gApplianceAdditionalInfos[15].minPower = 450.0;
    gApplianceAdditionalInfos[15].maxPower = 1500.0;
    gApplianceAdditionalInfos[15].supportedEnv = 255;
    //洗碗机
    gApplianceAdditionalInfos[16].id = 17;
    gApplianceAdditionalInfos[16].minUseTime = 20;
    gApplianceAdditionalInfos[16].maxUseTime = 180;
    gApplianceAdditionalInfos[16].useHourBitmapInDay = 3962624;
    gApplianceAdditionalInfos[16].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[16].maybeModeChange = 1;
    gApplianceAdditionalInfos[16].minPower = 1000.0;
    gApplianceAdditionalInfos[16].maxPower = 2200.0;
    gApplianceAdditionalInfos[16].supportedEnv = 1;
    //吸尘器
    gApplianceAdditionalInfos[17].id = 18;
    gApplianceAdditionalInfos[17].minUseTime = 0;
    gApplianceAdditionalInfos[17].maxUseTime = 10;
    gApplianceAdditionalInfos[17].useHourBitmapInDay = 4190080;
    gApplianceAdditionalInfos[17].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[17].maybeModeChange = 0;
    gApplianceAdditionalInfos[17].minPower = 350.0;
    gApplianceAdditionalInfos[17].maxPower = 1200.0;
    gApplianceAdditionalInfos[17].supportedEnv = 255;
    //大功率充电类
    gApplianceAdditionalInfos[18].id = 19;
    gApplianceAdditionalInfos[18].minUseTime = 15;
    gApplianceAdditionalInfos[18].maxUseTime = 300;
    gApplianceAdditionalInfos[18].useHourBitmapInDay = 16711683;
    gApplianceAdditionalInfos[18].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[18].maybeModeChange = 0;
    gApplianceAdditionalInfos[18].minPower = 90.0;
    gApplianceAdditionalInfos[18].maxPower = 300.0;
    gApplianceAdditionalInfos[18].supportedEnv = 255;
    //搅拌类（破壁机、豆浆机、榨汁机）
    gApplianceAdditionalInfos[19].id = 20;
    gApplianceAdditionalInfos[19].minUseTime = 0;
    gApplianceAdditionalInfos[19].maxUseTime = 5;
    gApplianceAdditionalInfos[19].useHourBitmapInDay = 1982336;
    gApplianceAdditionalInfos[19].useMonthBitmapInYear = 8190;
    gApplianceAdditionalInfos[19].maybeModeChange = 0;
    gApplianceAdditionalInfos[19].minPower = 70.0;
    gApplianceAdditionalInfos[19].maxPower = 300.0;
    gApplianceAdditionalInfos[19].supportedEnv = 1;
}

//TODO: low efficiency, optimize later
ApplianceAdditionalInfo* getApplianceAdditionalInfo(signed char id) {

    for (int i = 0; i < APPLIANCE_ADD_INFO_SIZE; i++) {
        if (id == gApplianceAdditionalInfos[i].id)
            return &(gApplianceAdditionalInfos[i]);
    }
    return NULL;
}

