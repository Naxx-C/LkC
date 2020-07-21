#include "arcfault_algo.h"
#include "file_utils.h"
#include "string_utils.h"
#include "time_utils.h"
#include <stdio.h>
#include "dirent.h"
#include <string.h>
#include <time.h>
#include <unistd.h>

static float current[128] = { -3.41796875, -3.41796875, -3.41796875, -3.46679688, -3.41796875, -3.41796875,
        -3.36914063, -3.515625, -3.27148438, -3.07617188, -2.83203125, -2.58789063, -2.39257813, -2.34375,
        -2.09960938, -2.00195313, -1.90429688, -1.66015625, -1.46484375, -1.12304688, -0.9765625,
        -0.830078125, -0.537109375, -0.439453125, -0.09765625, 0.09765625, 0.29296875, 0.341796875,
        0.341796875, 0.390625, 0.390625, 0.341796875, 0.341796875, 0.390625, 0.390625, 0.341796875,
        0.29296875, 0.341796875, 0.29296875, 0.341796875, 0.390625, 1.953125, 2.19726563, 2.24609375,
        2.44140625, 2.63671875, 2.78320313, 2.88085938, 2.88085938, 2.88085938, 2.97851563, 3.02734375,
        3.07617188, 3.22265625, 3.27148438, 3.27148438, 3.3203125, 3.3203125, 3.3203125, 3.27148438,
        3.27148438, 3.3203125, 3.27148438, 3.3203125, 3.3203125, 3.3203125, 3.36914063, 3.27148438, 3.3203125,
        3.3203125, 3.3203125, 3.17382813, 3.125, 2.97851563, 2.78320313, 2.63671875, 2.39257813, 2.34375,
        2.29492188, 2.05078125, 2.00195313, 1.85546875, 1.61132813, 1.41601563, 1.31835938, 1.12304688,
        0.927734375, 0.68359375, 0.48828125, 0.09765625, 0.146484375, 0.09765625, 0.09765625, 0.048828125,
        0.048828125, 0.048828125, 0.048828125, 0.048828125, 0.09765625, 0.048828125, 0.048828125, 0.146484375,
        0.146484375, 0.09765625, 0.146484375, 0, -0.09765625, -2.24609375, -2.34375, -2.49023438, -2.68554688,
        -2.78320313, -2.83203125, -2.83203125, -3.02734375, -3.07617188, -3.125, -3.22265625, -3.27148438,
        -3.41796875, -3.36914063, -3.41796875, -3.36914063, -3.36914063, -3.36914063, -3.46679688,
        -3.46679688, -3.46679688 };

static int parse(char *csvPath, int offset, float *currents, int len) {
    FILE *f = fopen(csvPath, "rb");
    int i = 0;
    int counter = 0;
    while (!feof(f) && !ferror(f)) {
        //    char line[100];
        //fgets(line, sizeof(line), f);
        //printf("%s\n", line);
        float cur, vol;
        fscanf(f, "%f,%f", &cur, &vol);
        if (i >= len)
            i = 0;
        currents[i++] = cur;
        counter++;
        printf("%f\n", cur);
    }
    fclose(f);
    return counter;
}

//char *dirPath = "F:\\data\\ArcfaultData\\20200410\\diantaolu_gaodijiaoti_falsealarm";
//char *dirPath = "F:\\data\\ArcfaultData\\20200410\\cleaner_gaodijiaoti_falsealarm";
//char *dirPath = "F:\\data\\ArcfaultData\\20200331_Cleaner\\xichenqi_no_arc";
//char *dirPath = "F:\\data\\ArcfaultData\\20200120_Diantaolu\\datacombine";
//char *dirPath = "F:\\data\\elecm\\0321_xichenqi";
//char *dirPath = "F:\\data\\ArcfaultData\\20200409\\cleaner_gaoditiaojie";
//char *dirPath = "F:\\data\\ArcfaultData\\20200409\\diantaolu_gaoditiaojie";
//char *dirPath = "F:\\data\\ArcfaultData\\20200409\\warmer_1k_arc";
//char *dirPath = "F:\\data\\ArcfaultData\\20200409\\warmer_2k_arc";
//char *dirPath = "F:\\data\\ArcfaultData\\20200409\\cleaner_max_arc";
//char *dirPath = "F:\\data\\ArcfaultData\\20200409\\clearner_res";
//char *dirPath = "F:\\data\\ArcfaultData\\20200305_Resistance\\res_1.73kw_join_to_apart";
//char *dirPath = "F:\\data\\ArcfaultData\\20200305_Resistance\\res_918w_join_to_apart\\";
//char *dirPath = "F:\\data\\ArcfaultData\\20200305_Resistance\\hairdryer_1.69kw_joint_to_apart";
//char *dirPath = "F:\\data\\ArcfaultData\\20200305_Resistance\\hairdryer_878kw_joint_to_apart";
//char *dirPath = "F:\\data\\ArcfaultData\\20191219_Cleaner\\";
//char *dirPath = "F:\\data\\ArcfaultData\\20191219_Cleaner_Resistor\\";
//char *dirPath = "F:\\data\\ArcfaultData\\20200331_Cleaner\\xichenqi_arc";
//char *dirPath = "F:\\data\\ArcfaultData\\20200331_Cleaner\\xichenqi_resistor_arc";
//char *dirPath = "F:\\data\\ArcfaultData\\20200410\\1kw_arc_alarm_normal";
//char *dirPath = "F:\\data\\ArcfaultData\\20200410\\2kw_arc_alarm_normal";
//char *dirPath = "F:\\data\\ArcfaultData\\20200410\\2kw_alarm_fail";
//char *dirPath = "F:\\data\\ArcfaultData\\20200410\\1kw_arc_alarm_fail";
//char *dirPath = "F:\\data\\ArcfaultData\\20200410\\cleaner_arc_alarm_normal";
static char *dirPath = "F:\\data\\ArcfaultData\\20200410\\clearner_res_alarm_normal";
static const int CHANNEL = 3;

int arcfault_main() {
    setArcAlarmThresh(14);
    setArcFftEnabled(0);
    arcAlgoInit(CHANNEL);
    // ArcFaultAlgo.setArcResJumpThresh(0.9f);
    // ArcFaultAlgo.setArcCheckDisabled(ArcFaultAlgo.ARC_CON_POSJ);
    setArcCheckDisabled(ARC_CON_PREJ); // Cleaner_Resistor must be
    setArcCheckDisabled(ARC_CON_POSJ);
    setArcOverlayCheckEnabled(1);

    char ret = arcAlgoInit(CHANNEL);

    if (ret > 1) {
        printf("some error\n");
        return -1;
    }
    DIR *dir = NULL;
    struct dirent *entry;

    int totalArc[CHANNEL], alarmNum[CHANNEL];
    memset(totalArc, 0, sizeof(int) * CHANNEL);
    memset(alarmNum, 0, sizeof(int) * CHANNEL);

//    while (1) {            //TODO:loop test, to remove
    int pointNum = 0, fileIndex = 0;
    if ((dir = opendir(dirPath)) == NULL) {
        printf("opendir failed\n");
    } else {
        char csv[500];
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
                    || !endWith(entry->d_name, ".csv")) ///current dir OR parrent dir
                continue;
            fileIndex = 0;
            memset(csv, 0, sizeof(csv));
            snprintf(csv, sizeof(csv) - 1, "%s\\%s", dirPath, entry->d_name);
//            printf("filepath=%s filetype=%d\n", csv, entry->d_type);        //输出文件或者目录的名称
            /**parse csv*/
            float currents[128];
            FILE *f = fopen(csv, "rb");
//            FILE *f = fopen(csvPath, "rb");
            int i = 0;

            while (!feof(f) && !ferror(f)) {
                float cur, vol;
                fscanf(f, "%f,%f", &cur, &vol);
                currents[i++] = cur;
                fileIndex++;
                if (i >= 128) {
                    int outArcNum[CHANNEL];
                    int arcNum1s[CHANNEL];
                    memset(outArcNum, 0, sizeof(int) * CHANNEL);
                    for (int channel = 0; channel < CHANNEL; channel++) {

                        char alarm = arcAnalyze(channel, currents, 128, &(arcNum1s[channel]),
                                &(outArcNum[channel]));

                        alarmNum[channel] += alarm;
                        totalArc[channel] += outArcNum[channel];
                        //if (alarm > 0)
                        if (outArcNum[channel] > 0) {
                            printf("file=%s index=%d num=%d\n", entry->d_name, fileIndex - 128,
                                    outArcNum[channel]);
                            // printf("arcNum1s=%d\n", arcNum1s[channel]);
                        }

                    }
                    i = 0;
                }
                pointNum++;
            }
            fclose(f);

        }
        for (int c = 0; c < CHANNEL; c++) {
            printf("alarmNum=%d totalArc=%d pointNum=%d\n", alarmNum[c], totalArc[c], pointNum);
        }
    }
    if (dir != NULL)
        closedir(dir);
    int t1 = printCurTime();
    sleep(3);
    int t2 = printCurTime();
    printf("%d %d %d", t1, t2, t2 - t1);
//    } //TODO:loop test, to remove

//    printf("version=%d\n", getArcAlgoVersion());
//    printf("%s %d %s %s", __FILE__, __LINE__, __DATE__, __TIME__);

    return 0;
}