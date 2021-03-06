#include "file_utils.h"
#include "string_utils.h"
#include "algo_set_build.h"
#include "algo_set_internal.h"
#include "func_arcfault.h"
#include "time_utils.h"
#include "algo_set.h"
#include <stdio.h>
#include <sys/stat.h>
#include "dirent.h"
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include "fft.h"
#ifdef _WIN32
#endif

//static char *onePath = "F:\\Tmp\\tiaoya";
//static char *onePath = "F:\\Tmp\\charging_yes";
//static char *onePath = "F:\\Tmp\\maliload";
//static char *onePath = "F:\\Tmp\\tiaoya_3";
//static char *onePath = "F:\\Tmp\\dingpinkongtiao";
//static char *onePath = "F:\\Tmp\\tiaoyawubao";
//static char *onePath = "F:\\Tmp\\fail";
static char gDirs[][100] = {
//        "F:\\data\\ArcfaultData\\category\\RealAlarm\\hairdryer",
//        "F:\\data\\ArcfaultData\\category\\FalseAlarm\\diancilu",
//        "F:\\data\\ArcfaultData\\category\\FalseAlarm\\tiaoyaqi",
//        "F:\\data\\DormConverter\\category\\FalseAlarm\\diantaolu",
//        "F:\\data\\DormConverter\\category\\RealAlarm",
//        "F:\\data\\DormConverter\\category\\MissAlarm",
//        "F:\\data\\ChargingAlarm\\category\\FalseAlarm"
//        "F:\\data\\DormConverter\\category\\FalseAlarm\\arcfault"
//          "F:\\Tmp\\0412dianbingxiangwubao",
//        "F:\\data\\DormConverter\\category\\FalseAlarm\\false_to_chongdianqi",
//        "F:\\Tmp\\maliload_yinshuiji"
//        "F:\\Tmp\\chuifengji"
//        "F:\\data\\maliload\\MissAlarm",
//        "F:\\data\\maliload\\MissAlarm\\jibian",
//        "F:\\data\\maliload\\FalseAlarm\\xiyiji_false_to_mali"
//        "F:\\data\\maliload\\MissAlarm\\diancilu"
//        "F:\\data\\devices\\fridge\\FalseAlarm_to_Maliload_min90W"
//        "F:\\data\\devices\\fridge\\FalseAlarm_to_Maliload_and_Charging_min30W"
//        "F:\\data\\devices\\fridge\\FalseAlarm"
//        "F:\\data\\devices\\fridge\\bug11681"
//        "F:\\data\\devices\\fridge"
//        "F:\\data\\devices\\humidifier\\0511"

//        "F:\\Tmp\\bug\\20210423\\fridge\\g3"
//        "F:\\Tmp\\bug\\20210423\\aux_350w\\True"
//        "F:\\Tmp\\bug\\20210423\\kangfu_dianchuifeng\\780w_yes"
//        "F:\\Tmp\\bug\\20210423\\fulizi_dianchuifeng\\l3_640W_yes"
//        "F:\\Tmp\\bug\\20210423\\fulizi_dianchuifeng\\l1_360w_yes"
//        "F:\\Tmp\\bug\\20210423\\dormconverter_false_alarm\\adjusting_arcfault"
//        "F:\\Tmp\\bug\\20210423\\dormconverter_false_alarm\\adjusting_dc_detected_then_arcfalt\\g2"
//        "F:\\Tmp\\bug\\20210506"//????????????
//        "F:\\data\\devices\\hairdrier\\miss"
//        "F:\\data\\devices\\warmer\\miss"
//        "F:\\data\\devices\\waterFountain"

//          "F:\\data\\devices\\charging\\true"
//        "F:\\data\\devices\\charging\\true\\first_batt_then_charger0"
        "F:\\data\\devices\\charging\\true\\first_charger_then_batt"
//        "F:\\data\\devices\\washer\\t2"
//        "F:\\data\\devices\\fridge\\FalseAlarm_to_Charging"
//        "F:\\Tmp\\bug\\20210427\\FalseAlarm_DC_To_Arcfault_bug11647"
//        "F:\\data\\ArcfaultData\\category\\RealAlarm\\20210429"
//        "F:\\data\\devices\\dormConverter\\FalseAlarm_to_Arcfault\\device1\\fix_level\\g1_L9"
        };

static int init() {

    int initRet = initTpsonAlgoLib();
    if (initRet != 0) {
        printf("initerr=%d\n", initRet);
    }

    setChargingAlarmSensitivity(0, CHARGING_ALARM_SENSITIVITY_MEDIUM);
    setMinEventDeltaPower(0, 20);
    setMinChargingDevicePower(0, 20);
//    setModuleEnable(ALGO_NILM_CLOUD_FEATURE, 1);
//    setModuleEnable(ALGO_DORM_CONVERTER_DETECT, 1);
    setModuleEnable(ALGO_CHARGING_DETECT, 1);
//    setModuleEnable(ALGO_MALICIOUS_LOAD_DETECT, 1);
//    setModuleEnable(ALGO_ARCFAULT_DETECT, 1);
//    setArcfaultSensitivity(0, ARCFAULT_SENSITIVITY_HIGH);
//    setDormConverterAlarmSensitivity(0, DORM_CONVERTER_SENSITIVITY_HIGH);

    for (int channel = 0; channel < CHANNEL_NUM; channel++) {
//        setChargingAlarmSensitivity(channel, CHARGING_ALARM_SENSITIVITY_MEDIUM);
//        setMaliLoadAlarmSensitivity(channel, MALI_LOAD_SENSITIVITY_HIGH);
//        setMinEventDeltaPower(channel, 55);
//        setMinChargingDevicePower(channel, 55);
    }
//    addMaliciousLoadWhitelist(0, 1000);
//    setMaliLoadWhitelistMatchRatio(0, 0.7, 1.3);
    return 0;
}

//static void sampleCall() {
//    //?????????
//    initTpsonAlgoLib();
//
//    //???????????????????????????
//    setModuleEnable(ALGO_CHARGING_DETECT, 1);
//    setModuleEnable(ALGO_DORM_CONVERTER_DETECT, 1);
//    setModuleEnable(ALGO_MALICIOUS_LOAD_DETECT, 1);
//    setModuleEnable(ALGO_ARCFAULT_DETECT, 1);
//
//    float current[128], voltage[128];
//    //?????????,????????????????????????buff???????????????
//    feedData(current, voltage, time(NULL), NULL);
//
//    //??????????????????
//    if(getChargingDetectResult()){
//        //do something
//    };
//    if(getDormConverterDetectResult()){
//        //do something
//    };
//    if(getMaliLoadDetectResult()){
//        //do something
//    };
//    int arcNum=0,onePeriodNum=0;
//    if(getArcfaultDetectResult(&arcNum,&onePeriodNum)){
//        //do something
//    };
//}

static float gSimulatedData[128] = { -2.8915193, -2.3132155, -2.3132155, -2.3132155, -2.0240636, -2.0240636,
        -2.0240636, -1.7349117, -1.7349117, -2.0240636, -1.7349117, -2.0240636, -1.4457597, -1.7349117,
        -1.7349117, -1.7349117, -1.4457597, -1.7349117, -1.4457597, -1.1566077, -1.1566077, -0.28915194,
        6.361343, 9.831166, 11.276926, 13.300989, 16.192509, 18.505724, 18.794876, 17.349115, 13.590142,
        9.542014, 6.361343, 4.626431, 4.048127, 4.048127, 4.337279, 3.758975, 3.1806715, 2.8915193, 1.4457597,
        0, -1.4457597, -1.1566077, -1.7349117, -1.4457597, -1.7349117, -1.1566077, -1.1566077, -1.4457597,
        -1.4457597, -1.7349117, -1.7349117, -2.0240636, -1.4457597, -2.0240636, -2.0240636, -2.0240636,
        -2.0240636, -2.0240636, -2.0240636, -2.0240636, -2.0240636, -2.3132155, -1.7349117, -2.0240636,
        -2.6023674, -2.6023674, -2.3132155, -2.6023674, -2.3132155, -2.6023674, -2.6023674, -2.6023674,
        -2.6023674, -2.6023674, -2.6023674, -2.6023674, -2.6023674, -3.1806715, -2.8915193, -2.6023674,
        -2.8915193, -3.1806715, -3.1806715, -4.915583, -10.987773, -13.590142, -15.614204, -17.92742,
        -21.108091, -23.132154, -23.132154, -21.108091, -18.505724, -14.168445, -11.276926, -9.252862,
        -8.3854065, -8.674558, -8.96371, -8.3854065, -7.807102, -7.228799, -6.0721908, -3.758975, -3.1806715,
        -2.8915193, -2.6023674, -2.6023674, -2.8915193, -2.8915193, -3.1806715, -2.8915193, -2.3132155,
        -2.6023674, -2.6023674, -2.3132155, -2.6023674, -2.8915193, -2.6023674, -2.6023674, -2.6023674,
        -2.6023674, -2.3132155, -2.8915193, -2.3132155, -2.6023674 };

int algo_set_test() {

//    printf("%d\n", sizeof(gDirs) / 100);
//    for (int i = 0; i < sizeof(gDirs) / 100; i++) {
//        char *dir = (char*) (gDirs + i);
//        printf("%s\n", dir);
//    }
//    return 0;

    for (int dirIndex = 0; dirIndex < sizeof(gDirs) / 100; dirIndex++) {
        char *dirPath = (char*) (gDirs + dirIndex);
        printf("\n\n\n=======%s=======\n", dirPath);

        init();
//        setArcLearningTime(dirIndex % CHANNEL_NUM, 3);
        DIR *dir = NULL;
        struct dirent *entry;

        int timeGap = 0;
        int pointNum = 0, fileIndex = 0;
        if ((dir = opendir(dirPath)) == NULL) {
            printf("opendir failed\n");
        } else {
            char csv[128];
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
                        || !endWith(entry->d_name, ".csv")) //016152951
                    continue;
//                initTpsonAlgoLib();
//                init();
                int convertHardcode = 1;
                if (startWith(entry->d_name, "elec_"))
                    convertHardcode = 1; //TODO:2020.11?????????????????????????????????????????????,??????-1

                fileIndex = 0;
                memset(csv, 0, sizeof(csv));
                snprintf(csv, sizeof(csv) - 1, "%s\\%s", dirPath, entry->d_name);
                printf("filepath=%s filetype=%d\n", csv, entry->d_type); //?????????????????????????????????
                /**parse csv*/
                float current[128], voltage[128];
                FILE *f = fopen(csv, "rb");
                printf("%s\n", csv);
//            FILE *f = fopen(csvPath, "rb");
                int i = 0;
                while (!feof(f) && !ferror(f)) {
                    float cur, vol;
                    fscanf(f, "%f,%f", &cur, &vol);
                    current[i] = cur * convertHardcode;
                    voltage[i] = vol;
                    fileIndex++;
                    i++;
                    if (i >= 128) {
                        setMaxChargingDevicePower(0, 5000);
                        feedData(dirIndex % CHANNEL_NUM, current, voltage, time(NULL) + (timeGap++) * 360,
                        NULL);
//                        feedData(dirIndex % CHANNEL_NUM, gSimulatedData, voltage, time(NULL), NULL);
                        i = 0;

                        NilmCloudFeature *nilmCloudFeature = NULL;
                        getNilmCloudFeature(0, &nilmCloudFeature); //nilmCloudFeature???????????????????????????????????????

                        if (nilmCloudFeature != NULL) {
                            printf("%f\n", nilmCloudFeature->activePower);
                        }
                    }
                    pointNum++;
                }
                fclose(f);
            }

        }
        if (dir != NULL)
            closedir(dir);
    }
    return 0;
}
