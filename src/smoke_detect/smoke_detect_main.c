#include<stdio.h>
#include"type_utils.h"
#include"smoke_detect.h"

static U8 gFrontRedAlarmThresh = 50; //差分的阈值
#define BUFF_SIZE 10
static U16 gFrontRedBuff[BUFF_SIZE] = { 0 };
static U16 gBackgroundValue[BUFF_SIZE] = { 0 };

static U16 gFrontRedCompare = 0;

static int gLastUnixTime = 0;

// 只插入一条，最新插入的数据排在buff后面
static void insertDataToBuff(U16 buff[], U8 buffSize, U16 newData) {

    U8 end = buffSize - 1;
    for (int i = 0; i < end; i++) {
        buff[i] = buff[i + 1];
    }
    buff[buffSize - 1] = newData;
}

// 针对进烟速度的惩罚 取值[0-50]
static U16 speedPulish(U16 data, U16 base) {
    int pulishR = 10;
    return (base + (data - base) * pulishR / 100);
}

static U16 getFrontRedCompareValue(U16 frontRed) {
    if (gFrontRedCompare == 0) { //首次运行
        gFrontRedCompare = frontRed;
    } else {
    }
}
/**
 * frontRed: 前向红光采样值
 * backgroundValue: 关对管值
 */

static char gIsFirst = 1;
static int gTimer = 0;
U8 smoke_detect(SAMPLE_VALUE sampleValue, int unixTime) {

    gTimer++;

    //防止因配置或校准时间带来的跳变
    if (gLastUnixTime > unixTime) {
        gLastUnixTime = 0;
    }

    U16 deltaFrontRed = sampleValue.frontRed - sampleValue.backgroundValue;
    insertDataToBuff(gFrontRedBuff, BUFF_SIZE, deltaFrontRed);

    if (gFrontRedCompare == 0) {
        gFrontRedCompare = sampleValue.frontRed;
    }

//    U16 deltaFrontBlue = frontBlue - backgroundValue;
//    U16 deltaBackRed = backRed - backgroundValue;
//    insertDataToBuff(gBackgroundValue, BUFF_SIZE, deltaBackRed);

    gIsFirst = 0;
    gLastUnixTime = unixTime;
    return 1;
}

void setFrontRedAlarmThresh(U8 frontRedAlarmThresh) {
    gFrontRedAlarmThresh = frontRedAlarmThresh;
}
