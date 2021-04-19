#include <math.h>
#include <nilm_appliance.h>
#include <nilm_idmap.h>
#include <nilm_onlinelist.h>
#include <stdio.h>
#include <string.h>
#include "log_utils.h"
#include "type_utils.h"
#include "algo_set.h"
#include "smoke_detect.h"
#include "algo_set_internal.h"

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

// 针对进烟速度的惩罚
static float speedPulish(float data, float base) {

//    return data;
    int pulishR = -500;
    return (base + (data - base) * (float) pulishR / 100);
}

static float gFrontRedBase = 0, gFrontRedAlarmThresh = 20;

static void updateFrontRedBaseValue(U16 frontRed) {

    if (gFrontRedBase == 0) { //首次运行
        gFrontRedBase = frontRed;
    } else {
        //异常过滤，如处于进烟状态
        //TODO:由于整除的精度问题，有可能永远不生效. (10, gFrontRedAlarmThresh / 2]之间的变化才能生效
        if (frontRed - gFrontRedBase < gFrontRedAlarmThresh / 2) {
            gFrontRedBase = (frontRed * 0.1 + gFrontRedBase * 99.9) / 100;
        }
    }

    if (outprintf != NULL)
        outprintf("%d: %f\n", frontRed, gFrontRedBase);
}

#define SIZE 100
#define  N 100

static NilmCloudFeature gNilmCloudFeature[3];
static void getNilmFeature2(int channel, NilmCloudFeature **nilmFeature) {

    *nilmFeature = gNilmCloudFeature + channel; //[channel];
    printf("%p\n", *nilmFeature);
}

static float f419[128] = { 0, 0.14648438, 0, 0.14648438, 0, 0, 0.07324219, 0.14648438, 0.07324219, 0.07324219,
        0.07324219, 0.07324219, 0.14648438, 0.14648438, 0.36621094, 0.6591797, 0.7324219, 0.6591797,
        1.0253906, 1.2451172, 1.3916016, 1.6113281, 1.9042969, 2.0507812, 2.2705078, 2.5634766, 2.6367188,
        2.7832031, 3.0761719, 3.149414, 3.2958984, 3.515625, 3.515625, 3.6621094, 3.515625, 3.7353516,
        3.7353516, 3.8085938, 3.881836, 3.9550781, 3.8085938, 3.881836, 3.8085938, 3.9550781, 3.9550781,
        3.9550781, 3.9550781, 4.0283203, 4.0283203, 3.9550781, 3.9550781, 3.881836, 3.881836, 3.8085938,
        3.881836, 3.7353516, 3.5888672, 3.3691406, 3.3691406, 3.2226562, 3.0761719, 3.149414, 3.0029297,
        2.6367188, 2.5634766, 2.4169922, 2.2705078, 2.0507812, 1.9042969, 1.9042969, 1.6113281, 1.3916016,
        1.2451172, 0.95214844, 0.87890625, 0.14648438, 0, -0.29296875, -0.29296875, -0.43945312, -0.29296875,
        -0.5859375, -0.6591797, -0.87890625, -1.0253906, -1.171875, -1.5380859, -1.4648438, -1.6113281,
        -1.8310547, -1.9775391, -2.0507812, -2.1240234, -2.2705078, -2.709961, -2.6367188, -2.709961,
        -2.709961, -2.8564453, -2.9296875, -2.9296875, -3.0029297, -3.0761719, -3.0761719, -3.0761719,
        -3.2226562, -3.149414, -3.0029297, -3.149414, -3.2226562, -3.2226562, -3.2226562, -3.0761719,
        -3.4423828, -3.2226562, -3.149414, -3.149414, -3.0029297, -2.9296875, -2.8564453, -2.709961,
        -2.6367188, -2.4902344, -2.34375, -2.2705078, -2.1240234, -1.9775391, -1.9042969 };
/**
 * 统计最大/最小的前几个数
 * 排序算法运算效率较低
 */
static void getTopDownAverage(float *data, int len, float *outTop, float *outDown) {

    float top[5] = { 0 }, down[5] = { 0 };
    float topAudit = top[0];
    float downAudit = down[0];
    float extreme = 0;
    char replaceFlag = 0;
    for (int i = 0; i < len; i++) {
        if (data[i] > topAudit) {
            extreme = 5000;
            replaceFlag = 0;
            for (int j = 0; j < 5; j++) {
                if (replaceFlag == 0 && topAudit == top[j]) {
                    top[j] = data[i]; //替换
                    replaceFlag = 1;
                }
                if (top[j] < extreme) { //找最小值
                    extreme = top[j];
                }
            }
            topAudit = extreme;
        } else if (data[i] < downAudit) {
            extreme = -5000;
            replaceFlag = 0;
            for (int j = 0; j < 5; j++) {
                if (replaceFlag == 0 && downAudit == down[j]) {
                    down[j] = data[i]; //替换
                    replaceFlag = 1;
                }
                if (down[j] > extreme) { //找最大值
                    extreme = down[j];
                }
            }
            downAudit = extreme;
        }
    }

    float topTmp = 0, downTmp = 0;
    for (int i = 0; i < 5; i++) {
        *outTop += top[i];
        if (top[i] > topTmp) {
            topTmp = top[i];
        }
        *outDown += down[i];
        if (down[i] < downTmp) {
            downTmp = down[i];
        }
    }
    *outTop = (*outTop - topTmp) / 4; //去除最大值
    *outDown = (*outDown - downTmp) / 4; //去除最小值
}

int main() {

    registerPrintf(printf);
    printf("start\n");

    setMaliLoadWhitelistMatchRatio(0, 0.8, 1.2);

//    for (int i = 0; i < 500; i++) {
//        U16 frontRed = i;
//        if (frontRed > 10)
//            frontRed = 10;
//        updateFrontRedBaseValue(frontRed);
//    }

    float outTop, outDown;
    getTopDownAverage(f419, 128, &outTop, &outDown);

    printf("%.3f %.3f\n", outTop, outDown);
    smoke_detect_test();

    //    float alarmThresh = 30;
//    float a02[N] = { 0 }, a10[N] = { 0 }, c02[N] = { 0 }, c10[N] = { 0 };
//
//    int alarmTime11 = 0, alarmTime12 = 0, alarmTime21 = 0, alarmTime22 = 0;
//    for (int i = 0; i < N; i++) {
//        a02[i] = i;
//        a10[i] = 1.6 * i;
//
//        if (a02[i] >= alarmThresh && alarmTime11 == 0)
//            alarmTime11 = i;
//        if (a10[i] >= alarmThresh && alarmTime12 == 0)
//            alarmTime12 = i;
//
//        if (i >= 6) {
//            c02[i] = speedPulish(a02[i], a02[i - 6]);
//            c10[i] = speedPulish(a10[i], a10[i - 6]);
//        }
//
//        if (c02[i] >= alarmThresh && alarmTime21 == 0)
//            alarmTime21 = i;
//        if (c10[i] >= alarmThresh && alarmTime22 == 0)
//            alarmTime22 = i;
//    }
//    printf("%d %d %d %d  ratio=%.1f vs %.1f\n", alarmTime11, alarmTime12, alarmTime21, alarmTime22,
//            (float) alarmTime11 / alarmTime12, (float) alarmTime21 / alarmTime22);

//    1.0 vs 1.4
    algo_set_test();

    printf("done\n");
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
