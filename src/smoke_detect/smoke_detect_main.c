#include<stdio.h>
#include"type_utils.h"
#include"log_utils.h"
#include"smoke_detect.h"

#define BUFF_SIZE 10
static U16 gFrontRedBuff[BUFF_SIZE] = { 0 }; //判断短期趋势，有烟雾时间隔5s，无烟雾间隔10s
static float gFrontRedBase = 0;

#define BUFF_30S_SIZE 6
static U16 gFrontRedBuff30S[BUFF_30S_SIZE] = { 0 }; //惩罚算法用

static U16 gFrontBlueBuff[BUFF_SIZE] = { 0 };
static float gFrontBlueBase = 0;

static int gLastUnixTime = 0;

//配置值
static U8 gFrontRedAlarmThresh = 20; //红前报警阈值: 相对本底值的变化量
static U8 gFrontBlueAlarmThresh = 10; //蓝前报警阈值: 相对本底值的变化量
static U16 gLightLeakageThresh = 200; //漏光判断阈值,关对管绝对值
static U16 gMazeFaultThresh = 200; //迷宫故障阈值,生产缺陷或污染,红外本底值

// 只插入一条，最新插入的数据排在buff后面
static void insertDataToBuff(U16 buff[], U8 buffSize, U16 newData) {

    U8 end = buffSize - 1;
    for (int i = 0; i < end; i++) {
        buff[i] = buff[i + 1];
    }
    buff[buffSize - 1] = newData;
}

// 针对进烟速度不同的惩罚算法, 取值[0-50]
static U16 smokeSamplePulish(U16 data, U16 base) {
    const static U16 PULISH_RATIO = 90;
    if (data > base) { //烟雾下降时不惩罚
        return (data - (data - base) * PULISH_RATIO / 100);
    }
    return data;
}

//更新本底值
/**
 *curBase:当前待更新的本底值
 *sampleValue:当前采样值
 *filterThresh:超过此阈值不作计算，防止真实进烟情况
 */
static void updateBaseValue(float *curBase, U16 sampleValue, U16 filterThresh) {

    if (*curBase == 0) { //首次运行
        *curBase = sampleValue;
    } else {
        //异常过滤，如处于进烟状态
        //如果curBase是整形，会由于整除的精度问题，有可能永远不生效. (10,filterThresh]之间的变化才能生效
        if (sampleValue - *curBase < filterThresh) {
            *curBase = (sampleValue * 0.1 + *curBase * 99.9) / 100;
        }
    }
//
//    if (outprintf != NULL)
//        outprintf("%d: %d\n", sampleValue, gFrontRedBase);
}
static char checkMazeFault(SMOKE_DETECT_RESULT *smokeDetectResult, SAMPLE_VALUE sampleValue) {
    static U8 lighLeakageCounter = 0, mazeFaultCounter = 0;
    //迷宫漏光
    if (sampleValue.backgroundValue > gLightLeakageThresh) {
        lighLeakageCounter++;
        if (lighLeakageCounter >= 3) {
            lighLeakageCounter = 3;
            smokeDetectResult->faultTypeBitmap |= (0x1 << LIGHT_LEAK);
        }
    } else {
        lighLeakageCounter = 0;
        smokeDetectResult->faultTypeBitmap &= ~(1 << LIGHT_LEAK);
    }
    //迷宫故障
    if (gFrontRedBase > gMazeFaultThresh) {
        mazeFaultCounter++;
        if (mazeFaultCounter >= 3) {
            mazeFaultCounter = 3;
            smokeDetectResult->faultTypeBitmap |= (0x1 << MAZE_FAULT);
        }
    } else {
        mazeFaultCounter = 0;
        smokeDetectResult->faultTypeBitmap &= ~(1 << MAZE_FAULT);
    }

    if (smokeDetectResult->faultTypeBitmap != 0)
        return 1;
    return 0;
}

static float gBigParticleRatioTresh = 3;
static char checkSmoke(SMOKE_DETECT_RESULT *smokeDetectResult, SAMPLE_VALUE sampleValue) {

    S16 deltaFrontRed = sampleValue.frontRed - gFrontRedBase;
    S16 deltaFrontBlue = sampleValue.frontBlue - gFrontBlueBase;

    //进烟速度惩罚
    U16 frontRedFinal = smokeSamplePulish(sampleValue.frontRed, gFrontRedBuff30S[BUFF_30S_SIZE - 2]); //2*30=1min

    //烟雾判断
    S16 deltaFrontRedFinal = frontRedFinal - gFrontRedBase;
    static char smokeCounter = 0; //计数器
    const static char SC_THRESH = 2;
    if (deltaFrontRedFinal > gFrontRedAlarmThresh) {

        if (smokeCounter < SC_THRESH)
            smokeCounter++;
    } else {
        if (smokeCounter > 0)
            smokeCounter--;
    }
    if (smokeCounter >= SC_THRESH) {
        smokeDetectResult->smokeTypeBitmap |= (0x1 << SMOKE_NORMAL);
    }

    //大颗粒判断
    static char bigParticleCounter = 0; //计数器
    const static char BPC_THRESH = 3;
    if (deltaFrontBlue != 0) {
        if (deltaFrontRed / deltaFrontBlue > gBigParticleRatioTresh) {
            if (bigParticleCounter < BPC_THRESH)
                bigParticleCounter++;
        } else {
            if (bigParticleCounter > 0)
                bigParticleCounter--;
        }
        if (bigParticleCounter >= BPC_THRESH) {
            smokeDetectResult->smokeTypeBitmap |= (0x1 << SMOKE_BIG_PARTICLE);
        }
    }
    return 0;
}

/**
 * 返回值:需不需要烟雾报警
 * frontRed: 前向红光采样值
 * backgroundValue: 关对管值
 */

static char gIsFirst = 1;
static int gTimer = 0;
char smoke_detect(SMOKE_DETECT_RESULT *smokeDetectResult, SAMPLE_VALUE sampleValue, int unixTime) {

    gTimer++;

    if (smokeDetectResult == NULL)
        return 0;
    //防止因配置或校准时间带来的跳变
    if (gLastUnixTime > unixTime) {
        gLastUnixTime = 0;
    }

    //buff更新
    insertDataToBuff(gFrontBlueBuff, BUFF_SIZE, sampleValue.frontBlue);
    insertDataToBuff(gFrontRedBuff, BUFF_SIZE, sampleValue.frontRed);
    static int lastFrontRed30SInsertTime = 0;
    if (unixTime - lastFrontRed30SInsertTime >= 28) {
        insertDataToBuff(gFrontRedBuff30S, BUFF_30S_SIZE, sampleValue.frontRed);
        lastFrontRed30SInsertTime = unixTime;
    }

    //更新本底值
    updateBaseValue(&gFrontRedBase, sampleValue.frontRed, gFrontRedAlarmThresh / 3);
    updateBaseValue(&gFrontBlueBase, sampleValue.frontBlue, gFrontBlueAlarmThresh);

    //故障判断
    char hasFault = checkMazeFault(smokeDetectResult, sampleValue);

    //烟雾报警判断
    if (!hasFault) {
        checkSmoke(smokeDetectResult, sampleValue);
    }

//    U16 deltaFrontBlue = frontBlue - backgroundValue;
//    U16 deltaBackRed = backRed - backgroundValue;
//    insertDataToBuff(gBackgroundValue, BUFF_SIZE, deltaBackRed);

    gIsFirst = 0;
    gLastUnixTime = unixTime;

    if ((smokeDetectResult->smokeTypeBitmap & (0x1 << SMOKE_NORMAL)) != 0)
        return 1;
    return 0;
}

void setFrontRedAlarmThresh(U8 thresh) {
    gFrontRedAlarmThresh = thresh;
}

void setLightLeakageThresh(U8 thresh) {
    gLightLeakageThresh = thresh;
}

void setMazeFaultThresh(U8 thresh) {
    gMazeFaultThresh = thresh;
}

void setBigParticleRatioTresh(float thresh) {
    gMazeFaultThresh = thresh;
}
