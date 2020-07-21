#include <stdio.h>
#include "dirent.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "nilm_appliance.h"
#include "nilm_algo.h"
#include "nilm_idmap.h"
#include "nilm_onlinelist.h"

const char *APP_BUILD_DATE = __DATE__;
extern int arcfault_main();
extern int nilm_algo_main();
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

float RadiantCooker[8] = { 3.62, 0.06, 0.18, 0.07, 0.08, 0.99, 1.05, 1129.82 };
float Cleaner[8] = { 3.05, 0.35, 0.12, 0.05, 0.08, 0.99, 6.22, 983.61 };
float Microwave[8] = { 3.08, 0.97, 0.29, 0.11, 0.06, 0.87, 1.27, 933.31 };
float Microwave2[8] = { 1.85, 1.00, 0.35, 0.04, 0.05, 0.31, 3.05, 202.64 };
extern NilmAppliance gNilmAppliances[100];

#define MAX_ID_MAP 10
IdMap gIdMaps[MAX_ID_MAP];

float current[128];
float voltage[128];
float oddFft[5] = { 3.08, 0.97, 0.29, 0.11, 0.06 };

void setFakeValue(float data[128],float val) {
    for(int i=0;i<128;i++){
        data[i] = val;
    }
}
int main() {

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

    OnlineAppliance o1,o2,o3,o4,o5;
    o1.activePower=1000;
    o1.id=1;
    o1.possiblity=0.8;
    o1.poweronTime=100086;
    o1.voltage = 200;

    o2.activePower=1000;
    o2.id=2;
    o2.possiblity=0.8;
    o2.poweronTime=100086;
    o2.voltage = 200;

    o3.activePower=1000;
    o3.id=2;
    o3.possiblity=0.8;
    o3.poweronTime=100086;
    o3.voltage = 200;

    o4.activePower=1000;
    o4.id=2;
    o4.possiblity=0.8;
    o4.poweronTime=100086;
    o4.voltage = 200;

    o5.activePower=-900;
    o5.id=3;
    o5.possiblity=0.8;
    o5.poweronTime=100086;
    o5.voltage = 200;

    addToOnlineList(&o1);
    addToOnlineList(&o2);
    addToOnlineList(&o3);
    addToOnlineList(&o4);
    addToOnlineList(&o5);
    printf("%f\n",getOnlineListPower());
    updateOnlineAppPower(220);
    printf("%f\n",getOnlineListPower());

    MatchedAppliance m1,m2,m3,m4,m5;
    m1.activePower=1000;
    m1.id=1;
    m1.possiblity=0.8;

    m2.activePower=1000;
    m2.id=2;
    m2.possiblity=0.8;

    m3.activePower=1000;
    m3.id=2;
    m3.possiblity=0.85;

    m4.activePower=1000;
    m4.id=2;
    m4.possiblity=0.79;

    m5.activePower=1000;
    m5.id=3;
    m5.possiblity=0.9;

    addToMatchedList(&m1);
    addToMatchedList(&m2);
    addToMatchedList(&m3);
    addToMatchedList(&m4);
    addToMatchedList(&m5);

    signed char bestMatchedId = -1;
    float possibility = 0;
    getBestMatchedOnline(1000, &bestMatchedId, &possibility);
    printf("end\n");
    return 0;
}
