#include "file_utils.h"
#include "string_utils.h"
#include "algo_set_build.h"
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

static char gDirs[][100] = {
//        "F:\\Tmp\\tiaoya_3lk2", "F:\\Tmp\\tiaoyalk2", "F:\\Tmp\\charginglk2",
//        "F:\\Tmp\\bianpinkongtiaolk2", "F:\\Tmp\\maliload_diancilulk2", "F:\\Tmp\\maliload_zhudanqilk2",
//        "F:\\Tmp\\maliload_dianchuifenglk2", "F:\\Tmp\\maliload_reshuiqilk2",
//        "F:\\Tmp\\charging_laptop_wubaolk2", "F:\\Tmp\\charging_misslk2", "F:\\Tmp\\charging_wubaolk2",
//        "F:\\data\\ArcfaultData\\20200409\\warmer_2k_arc", "F:\\Tmp\\dorm_falsealarm_charginglk2",
//        "F:\\Tmp\\charging_medium_lowratelk2",
//        "F:\\Tmp\\arcfault_falsealarm\\diantaolu",
//        "F:\\data\\ArcfaultData\\category\\RealAlarm\\hairdryer",
//        "F:\\data\\ArcfaultData\\category\\FalseAlarm\\diancilu",
        "F:\\data\\ArcfaultData\\category\\FalseAlarm\\tiaoyaqi",
//        "F:\\data\\DormConverter\\category\\FalseAlarm\\diantaolu",
//        "F:\\data\\DormConverter\\category\\RealAlarm",
        };

static int init() {
    return 0;
}

// 针对进烟速度的惩罚
static float speedPulish(float data, float base) {

//    return data;
    int pulishR = 10;
    return (base + (data - base) * (float) pulishR / 100);
}
#define  N 100
static void speedPulishTest() {

    float alarmThresh = 20;
    float a02[N] = { 0 }, a10[N] = { 0 }, c02[N] = { 0 }, c10[N] = { 0 };

    int alarmTime11 = 0, alarmTime12 = 0, alarmTime21 = 0, alarmTime22 = 0;
    for (int i = 0; i < N; i++) {
        a02[i] = i;
        a10[i] = 2 * i;

        if (a02[i] >= alarmThresh && alarmTime11 == 0)
            alarmTime11 = i;
        if (a10[i] >= alarmThresh && alarmTime12 == 0)
            alarmTime12 = i;

        if (i >= 6) {
            c02[i] = speedPulish(a02[i], a02[i - 6]);
            c10[i] = speedPulish(a10[i], a10[i - 6]);
        }

        if (c02[i] >= alarmThresh && alarmTime21 == 0)
            alarmTime21 = i;
        if (c10[i] >= alarmThresh && alarmTime22 == 0)
            alarmTime22 = i;
    }
    printf("%d %d %d %d  ratio=%.1f vs %.1f\n", alarmTime11, alarmTime12, alarmTime21, alarmTime22,
            (float) alarmTime11 / alarmTime12, (float) alarmTime21 / alarmTime22);
}

int smoke_detect_test() {

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
                    convertHardcode = 1; //TODO:2020.11月之前的，单相电抓的数据是反的,需要-1

                fileIndex = 0;
                memset(csv, 0, sizeof(csv));
                snprintf(csv, sizeof(csv) - 1, "%s\\%s", dirPath, entry->d_name);
                printf("filepath=%s filetype=%d\n", csv, entry->d_type); //输出文件或者目录的名称
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
