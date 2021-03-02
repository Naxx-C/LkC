#include <math.h>
#include <nilm_appliance.h>
#include <nilm_idmap.h>
#include <nilm_onlinelist.h>
#include <stdio.h>
#include <string.h>
#include "log_utils.h"

typedef struct {
    int a;
} TestA;

typedef struct {
    int b;
    TestA testA;
} TestB;

void f(TestB *testB) {
    TestA *testA = &(testB->testA);
    testB->b = 22;
    testA->a = 11;
}

//float RadiantCooker[8] = { 3.62, 0.06, 0.18, 0.07, 0.08, 0.99, 1.05, 1129.82 };
//float Cleaner[8] = { 3.05, 0.35, 0.12, 0.05, 0.08, 0.99, 6.22, 983.61 };
//float Microwave[8] = { 3.08, 0.97, 0.29, 0.11, 0.06, 0.87, 1.27, 933.31 };
//float Microwave2[8] = { 1.85, 1.00, 0.35, 0.04, 0.05, 0.31, 3.05, 202.64 };

float Cleaner1[8] = { 3.400, 2.600, 1.400, 0.700, 0.500, 0.550, 4.300, 382.300 };
float Cleaner2[8] = { 6.700, 1.000, 0.300, 0.100, 0.100, 0.990, 7.100, 1028.400 };
float Cleaner3[8] = { 3.900, 2.900, 1.400, 0.700, 0.400, 0.530, 3.800, 418.500 };
float Cleaner4[8] = { 7.800, 1.400, 0.300, 0.100, 0.100, 0.980, 5.600, 1198.000 };
extern NilmAppliance gNilmAppliances[100];

#define MAX_ID_MAP 10
IdMap gIdMaps[MAX_ID_MAP];

float current[128];
float voltage[128];
float oddFft[5] = { 3.08, 0.97, 0.29, 0.11, 0.06 };

void setFakeValue(float data[128], float val) {
    for (int i = 0; i < 128; i++) {
        data[i] = val;
    }
}

#define ALL_FILE_NUM  100  //文件个数

#pragma pack(1)

typedef struct {
    signed char id; //ID号
    char name[16]; //电器名称
    float oddFft[5]; //特征参数
    float powerFactor;
    float pulseI;
    float activePower;
} FeatureData_TypeDef;

#pragma pack()

//结构体赋值 ，一个文件对应一个结构体,各数组中的ID、电器名称可重复
FeatureData_TypeDef FeatureData[ALL_FILE_NUM];
void initFeature() {
    FeatureData[0].id = 1;
    strncpy(FeatureData[0].name, "我是中国人", 15);
    FeatureData[0].oddFft[0] = 1;
    FeatureData[0].oddFft[1] = 1;
    FeatureData[0].oddFft[2] = 1;
    FeatureData[0].oddFft[3] = 1;
    FeatureData[0].oddFft[4] = 1;
    FeatureData[0].powerFactor = 1;
    FeatureData[0].pulseI = 1;
    FeatureData[0].activePower = 1;
}

typedef struct {

    float a; // delta active power
    int b;
    char c[2]; // powerline's total power
} __attribute__ ((packed)) TEST;

TEST test[3];

int x921[3];
void getX(int **y) {
    *y = x921;
}

const char *APP_BUILD_DATE = __DATE__;
extern void setArcCurrentRange(float minCurrent, float maxCurrent);

struct printf_operations {
    int (*arc_printf)(const char*, ...);
};
struct printf_operations arc_ops = { .arc_printf = printf,

};

#define CHANNEL_NUM 4
#define HOUR 10
#define ARC_TIME_TRIGGER_BUFFSIZE 10
static char gSmartmodeTimeTrigger[CHANNEL_NUM][ARC_TIME_TRIGGER_BUFFSIZE] = { 0 }; //24h内有4次触发(1个小时内重复触发不计)
static int gTimeTriggerBuffIndex[CHANNEL_NUM] = { 0 };
static int gLastTimeTriggerSectionTime[CHANNEL_NUM] = { 0 };
static int gLastTimeTriggerAlarmAuditTime[CHANNEL_NUM] = { 0 };

static int getSmartModeTimeTriggerTimes(int channel, int alarmAction, int unixTime) {
    if (unixTime - gLastTimeTriggerSectionTime[channel] >= HOUR) {
        if (gTimeTriggerBuffIndex[channel] >= ARC_TIME_TRIGGER_BUFFSIZE
                || gTimeTriggerBuffIndex[channel] < 0) {
            gTimeTriggerBuffIndex[channel] = 0;
        }
        if (alarmAction == 1) {
            if (unixTime - gLastTimeTriggerAlarmAuditTime[channel] >= HOUR) {
                gSmartmodeTimeTrigger[channel][gTimeTriggerBuffIndex[channel]] = 1;
                gLastTimeTriggerAlarmAuditTime[channel] = unixTime;
            }
        } else {
            gSmartmodeTimeTrigger[channel][gTimeTriggerBuffIndex[channel]] = 0;
        }

        gTimeTriggerBuffIndex[channel]++;
        gLastTimeTriggerSectionTime[channel] = unixTime; //上一次处理记录时间
    } else if (alarmAction == 1) {
        int lastIndex = gTimeTriggerBuffIndex[channel] - 1;
        if (lastIndex < 0) {
            lastIndex = ARC_TIME_TRIGGER_BUFFSIZE - 1;
        }
        if (gSmartmodeTimeTrigger[channel][lastIndex]
                == 0&& unixTime-gLastTimeTriggerAlarmAuditTime[channel]>=HOUR) {
            gSmartmodeTimeTrigger[channel][lastIndex] = 1;
            gLastTimeTriggerAlarmAuditTime[channel] = unixTime;
        }
    }

    int smartmodeTimeTrigger = 0;
    for (int i = 0; i < ARC_TIME_TRIGGER_BUFFSIZE; i++) {
        smartmodeTimeTrigger += gSmartmodeTimeTrigger[channel][i];
    }

    return smartmodeTimeTrigger;
}

static void f2(int a){
    static int b=0;
    b += a;
    printf("%d\n",b);
}

#define SIZE 100
int main() {

//    int alarmAction[SIZE] = {//
//            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
//            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
//            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
//            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
//            1, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
//            0, 0, 0, 0, 1, 0, 0, 0, 0, 0, //
//            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
//            0, 1, 0, 0, 0, 0, 0, 0, 0, 0, //
//            0, 0, 0, 0, 1, 0, 0, 0, 0, 0, //
//            0, 0, 0, 0, 0, 0, 1, 0, 0, 0, //
//            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
//            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
//            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
//            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
//            };
//    for (int i = 0; i < SIZE; i++) {
//        int smartmodeTimeTrigger = getSmartModeTimeTriggerTimes(0, alarmAction[i % SIZE], 100 + i);
//        printf("%d: %d\n", i + 1, smartmodeTimeTrigger);
//    }

    for (int i = 0; i < 3; i++)
        f2(1);
    //24小时内触发4次以上
    registerPrintf(printf);

//    if (algoset_printf != NULL)
//        algoset_printf("hello %d\n", 1);
//    setArcCurrentRange(1.0f, 4.0f);
//    algo_set_test();
    return 0;

//    charging_alarm_main();
//    dorm_converter_main();

    float cur[128] = { 0.29296875, 0, 0.341796875, 0.341796875, 0.1953125, 0.244140625, 0.1953125,
            0.146484375, 0.09765625, 0.146484375, 0, 0, 0.29296875, -0.048828125, 0.048828125, 0.29296875,
            0.29296875, 0.341796875, 0.1953125, 0.390625, 1.22070313, 1.66015625, 2.44140625, 2.734375,
            3.07617188, 2.34375, 2.88085938, 1.85546875, 2.19726563, 1.26953125, 0.9765625, 2.34375,
            1.70898438, 1.61132813, 0.68359375, 0.634765625, 0.48828125, 0.1953125, 0.390625, 0.146484375, 0,
            -0.048828125, 0.244140625, 0.146484375, 0.341796875, -0.09765625, 0, -0.048828125, 0.1953125,
            0.341796875, -0.048828125, -0.244140625, 0.29296875, -0.48828125, -0.244140625, -0.146484375, 0,
            -0.048828125, 0.048828125, -0.048828125, -0.09765625, -0.09765625, -0.09765625, -0.048828125,
            -0.048828125, -0.146484375, -0.048828125, -0.146484375, -0.09765625, 0, -0.09765625, -0.09765625,
            -0.048828125, 0.048828125, 0.244140625, 0.1953125, -0.146484375, -0.390625, -0.68359375,
            0.146484375, 0.048828125, -0.29296875, -0.09765625, -0.048828125, -0.9765625, -1.7578125,
            -2.09960938, -2.734375, -2.63671875, -2.34375, -1.85546875, -2.09960938, -1.90429688, -0.390625,
            -0.87890625, -1.3671875, -0.09765625, -0.87890625, 0.830078125, -0.634765625, -0.29296875,
            -0.78125, 0.48828125, 0.732421875, -0.048828125, 0.09765625, 0.09765625, -0.09765625, 0.048828125,
            -0.146484375, -0.09765625, 0.048828125, -0.09765625, -0.09765625, -0.146484375, 0.048828125,
            -0.09765625, -0.09765625, 0, -0.146484375, 0, -0.048828125, -0.048828125, -0.09765625, 0.09765625,
            -0.048828125, -0.09765625, 0.048828125 };
    int BATCH = 4, curStart = 0;
    float max = -999, min = 999;
    int maxIndex = 0, minIndex = 0;
    float c[33]; // 扩充1位
    memset(&c, 0, sizeof(c));
    for (int i = 0; i < 128; i += BATCH) {
        for (int j = 0; j < BATCH; j++) {
            c[i / BATCH] += cur[curStart + i + j] / BATCH;
        }
        if (c[i / BATCH] > max) {
            max = c[i / BATCH];
            maxIndex = i / BATCH;
        } else if (c[i / BATCH] < min) {
            min = c[i / BATCH];
            minIndex = i / BATCH;
        }
    }
    c[32] = c[0];
    float ad = 0, delta, average = 0;
    for (int i = 1; i < 33; i++) {
        delta = (float) (c[i] - c[i - 1]);
        delta = delta > 0 ? delta : -delta;
        ad += delta;
        if (c[i] > max)
            max = c[i];
    }
    ad /= 32;

    for (int i = 0; i < 32; i++) {
        average += c[i];
        printf("%.2f\t", c[i]);
    }
    printf("\n");
    average /= 32;
    int direction = 0, extremeNum = 0, flatNum = 0, flatBitmap = 0;
    float valueThresh = max / 9, deltaThresh = ad / 2;
    for (int i = 0; i < 32; i++) {
        // 周边至少一个点也是平肩
        if ((i >= 1 && fabs(c[i]) < valueThresh && fabs(c[i - 1]) < valueThresh
                && fabs(c[i] - c[i - 1]) < deltaThresh)
                || (i <= 31 && fabs(c[i]) < valueThresh && fabs(c[i + 1]) < valueThresh
                        && fabs(c[i + 1] - c[i]) < deltaThresh)) {
            flatNum++;
            flatBitmap = flatBitmap | (0x1 << i);
            printf("%d ", i);
        }
    }
// 极值点个数.极值点非平肩,平肩不切换方向
    for (int i = 0; i < 32; i++) {
        if (c[i + 1] - c[i] > 0 && (flatBitmap & ((0x1 << (i + 1)) | (0x1 << i))) == 0) {
            if (direction < 0) {
                extremeNum++;
                printf("\next:%d", i);
            }
            direction = 1;
        } else if (c[i + 1] - c[i] < 0 && (flatBitmap & ((0x1 << (i + 1)) | (0x1 << i))) == 0) {
            if (direction > 0) {
                extremeNum++;
                printf("\next:%d", i);
            }
            direction = -1;
        }
    }
    printf("\n%d %d\n", extremeNum, flatNum);

//    printf("%d\n",getCurTime());
//    nilm_main();
//    struct timeval tv_begin, tv_end;
//    gettimeofday(&tv_begin, NULL);
//    gettimeofday(&tv_end, NULL);
//    printf("%ld\n",tv_end.tv_usec);
//    printf("%ld %ld\n", (tv_end.tv_sec - tv_begin.tv_sec),(tv_end.tv_usec - tv_begin.tv_usec));
//    arcfault_main();
    return 0;
//    addToIdMap(gIdMaps, sizeof(gIdMaps) / sizeof(IdMap), 1, "RadiantCooker", strlen("RadiantCooker"));
//    addToIdMap(gIdMaps, sizeof(gIdMaps) / sizeof(IdMap), 2, "Cleaner", strlen("Cleaner"));
//    addToIdMap(gIdMaps, sizeof(gIdMaps) / sizeof(IdMap), 3, "Microwave", strlen("Microwave"));
//
//    char name[APP_NAME_LEN];
//    getNameInIdMap(gIdMaps, MAX_ID_MAP, 1, name);
//    NilmAppliance *na = createNilmAppliance(name, APP_NAME_LEN, 1, 1.5f);
//    addNormalizedFeature(na, RadiantCooker);
//
//    na = createNilmAppliance("Cleaner", sizeof("Cleaner"), 2, 2.5f);
//    addNormalizedFeature(na, Cleaner);
//    na = createNilmAppliance("Microwave", strlen("Microwave"), 3, 3.5f);
//    addNormalizedFeature(na, Microwave);
//    addNormalizedFeature(na, Microwave2);
//
//    setFakeValue(voltage, 220);
//    for (int i = 0; i < 100000; i++) {
//
//        if (i < 300)
//            setFakeValue(current,1);
//        if (i >= 300 && i < 600)
//            setFakeValue(current,3);
//        if (i >=600 && i < 1000)
//            setFakeValue(current,6);
//        if (i >= 1000 && i < 2000)
//            setFakeValue(current, 9);
//        nilmAnalyze(current, voltage, 128, 1593589351, -1, -1, -1, oddFft);
//    }

    OnlineAppliance o1, o2, o3, o4, o5;
    o1.activePower = 1000;
    o1.id = 1;
    o1.possiblity = 0.8;
    o1.poweronTime = 100086;
    o1.voltage = 200;

    o2.activePower = 1000;
    o2.id = 2;
    o2.possiblity = 0.8;
    o2.poweronTime = 100086;
    o2.voltage = 200;

    o3.activePower = 800;
    o3.id = 2;
    o3.possiblity = 0.8;
    o3.poweronTime = 100086;
    o3.voltage = 200;

    o4.activePower = 700;
    o4.id = 2;
    o4.possiblity = 0.8;
    o4.poweronTime = 100086;
    o4.voltage = 200;

    o5.activePower = 900;
    o5.id = 3;
    o5.possiblity = 0.8;
    o5.poweronTime = 100086;
    o5.voltage = 200;

    updateOnlineListByEvent(&o1);
    updateOnlineListByEvent(&o2);
    updateOnlineListByEvent(&o3);
    updateOnlineListByEvent(&o4);
    updateOnlineListByEvent(&o5);

    /***
     * 识别流程开始
     */
//根据最新电压更新在线电器列表的功率
    updateOnlineListPowerByVol(220);
    MatchedAppliance m1, m2, m3, m4, m5;
    m1.activePower = 1000;
    m1.id = 1;
    m1.possiblity = 0.8;

    m2.activePower = 1000;
    m2.id = 2;
    m2.possiblity = 0.8;

    m3.activePower = 1000;
    m3.id = 2;
    m3.possiblity = 0.85;

    m4.activePower = 1000;
    m4.id = 2;
    m4.possiblity = 0.79;

    m5.activePower = 1000;
    m5.id = 9;
    m5.possiblity = 0.9;

//本次的候选识别结果
    addToMatchedList(&m1);
    addToMatchedList(&m2);
    addToMatchedList(&m3);
    addToMatchedList(&m4);
    addToMatchedList(&m5);

//选取最大概率的识别结果
    signed char bestMatchedId = -1;
    float possibility = 0;
    getBestMatchedApp(-1000, &bestMatchedId, &possibility);

    OnlineAppliance o;
    o.activePower = -1000;
    o.id = bestMatchedId;
    o.possiblity = possibility;
    o.poweronTime = 123405;
    o.voltage = 210;

    updateOnlineListByEvent(&o);
    powerCheck(100.0f);

    printf("end\n");
    return 0;
}
