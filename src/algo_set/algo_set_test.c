#include "file_utils.h"
#include "string_utils.h"
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

//static char *dirPath = "F:\\Tmp\\tiaoya";
//static char *dirPath = "F:\\Tmp\\charging_yes";
//static char *dirPath = "F:\\Tmp\\maliload";
//static char *dirPath = "F:\\Tmp\\tiaoya_3";
//static char *dirPath = "F:\\Tmp\\dingpinkongtiao";
//static char *dirPath = "F:\\Tmp\\tiaoyawubao";
static char *dirPath = "F:\\Tmp\\fail";

static int init() {

    initTpsonAlgoLib();
    setModuleEnable(ALGO_CHARGING_DETECT, 0);
    setModuleEnable(ALGO_DORM_CONVERTER_DETECT, 0);
    setModuleEnable(ALGO_MALICIOUS_LOAD_DETECT, 1);
    setModuleEnable(ALGO_ARCFAULT_DETECT, 0);
    return 0;
}

//static void sampleCall() {
//    //初始化
//    initTpsonAlgoLib();
//
//    //使能需要的算法模块
//    setModuleEnable(ALGO_CHARGING_DETECT, 1);
//    setModuleEnable(ALGO_DORM_CONVERTER_DETECT, 1);
//    setModuleEnable(ALGO_MALICIOUS_LOAD_DETECT, 1);
//    setModuleEnable(ALGO_ARCFAULT_DETECT, 1);
//
//    float current[128], voltage[128];
//    //送数据,传入电流电压采样buff的起始指针
//    feedData(current, voltage, time(NULL), NULL);
//
//    //获取算法结果
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

int algo_set_test() {

    init();
    DIR *dir = NULL;
    struct dirent *entry;

    int pointNum = 0, fileIndex = 0;
    if ((dir = opendir(dirPath)) == NULL) {
        printf("opendir failed\n");
    } else {
        char csv[128];
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
                    || !endWith(entry->d_name, ".csv")) //016152951
                continue;
            initTpsonAlgoLib();
            int convertHardcode = 1;
            if (startWith(entry->d_name, "elec_"))
                convertHardcode = 1; //TODO:11月之前的，单相电抓的数据是反的,需要-1

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
                    feedData(current, voltage, time(NULL), NULL);
                    i = 0;
                }
                pointNum++;
            }
            fclose(f);
        }

    }
    if (dir != NULL)
        closedir(dir);

    return 0;
}
