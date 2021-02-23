/***************************************************************************************
 ****************************************************************************************
 * FILE     : arcStudy.c
 * Description  :
 *
 * Copyright (c) 2021 by Tpson. All Rights Reserved.
 *
 * History:
 * Version      Name            Date            Description
 0.1       HuangDong        2021/01/14      Initial Version

 ****************************************************************************************
 ****************************************************************************************/

#include <arcfault_smartmode.h>
#include "stddef.h"
#include "stdio.h"
#include "math.h"
#include "math_utils.h"
#include "string.h"
#include "stdbool.h"
#include "algo_set_build.h"

//支持最大通道数
#define MAX_CHANNEL_NUM CHANNEL_NUM
//特征数据组个数
#define MAX_FEATURE_STUDY_NUM 10
//缓存电弧个数
#define MAX_FEATURE_CACHE_NUM 3

#pragma pack(1)
//电弧特征数据
typedef struct _tagArcFeature {
    float Area[3]; //面积特征
    float effCurrent; //有效电流
    float Res[4]; //预留参数
} ArcFeature_TypeDef, *pArcFeature_TypeDef;
#pragma pack()

/*学习的特征数据*/
static ArcFeature_TypeDef ArcFeatureData[MAX_CHANNEL_NUM][MAX_FEATURE_STUDY_NUM] = { 0 };
/*缓存特征数据*/
static ArcFeature_TypeDef ArcCacheData[MAX_CHANNEL_NUM][MAX_FEATURE_CACHE_NUM] = { 0 };

/*flash操作接口*/
struct flash_operations arc_ops;

//初始化注册
int ArcfaultFlashOpsRegister(struct flash_operations *ops) {
    uint32_t flag_start = 0;
    uint32_t flag_end = 0;

    if (ops == NULL || ops->save_addr == NULL || ops->flash_buff_write == NULL || ops->flash_buff_read == NULL) {
        return -1;
    }

    arc_ops.save_addr = ops->save_addr;
    arc_ops.flash_erase = ops->flash_erase;
    arc_ops.flash_buff_write = ops->flash_buff_write;
    arc_ops.flash_buff_read = ops->flash_buff_read;

    arc_ops.flash_buff_read((uint8_t*) &flag_start, arc_ops.save_addr, 4);
    arc_ops.flash_buff_read((uint8_t*) &flag_end, arc_ops.save_addr + 4 + sizeof(ArcFeatureData), 4);

    if (flag_start == 0xAAAAAAAA && flag_end == 0x55555555) {
        arc_ops.flash_buff_read((uint8_t*) &ArcFeatureData, arc_ops.save_addr + 4, sizeof(ArcFeatureData));
//        for(int i = 0; i < MAX_FEATURE_STUDY_NUM;i++)
//        {
//            arc_printf("\r\nArea1: %.1f\r\nArea2: %.1f\r\nArea3: %.1f\r\nIa:%.1f\r\n",
//                ArcFeatureData[0][i].Area[0],ArcFeatureData[0][i].Area[1],ArcFeatureData[0][i].Area[2],ArcFeatureData[0][i].effCurrent);
//        }
        return 1;
    } else {
        flag_start = 0xAAAAAAAA;
        flag_end = 0x55555555;
        arc_ops.flash_erase(arc_ops.save_addr);
        arc_ops.flash_buff_write((uint8_t*) &flag_start, arc_ops.save_addr, 4);
        arc_ops.flash_buff_write((uint8_t*) &ArcFeatureData, arc_ops.save_addr + 4, sizeof(ArcFeatureData));
        arc_ops.flash_buff_write((uint8_t*) &flag_end, arc_ops.save_addr + 4 + sizeof(ArcFeatureData), 4);
    }
    return 0;
}

/*保存特征数据*/
static void SaveArcFeature(void) {
    uint32_t flag_start = 0xAAAAAAAA;
    uint32_t flag_end = 0x55555555;

    if (arc_ops.flash_erase == NULL || arc_ops.flash_buff_write == NULL) {
        return;
    }
    arc_ops.flash_erase(arc_ops.save_addr);
    arc_ops.flash_buff_write((uint8_t*) &flag_start, arc_ops.save_addr, 4);
    arc_ops.flash_buff_write((uint8_t*) &ArcFeatureData, arc_ops.save_addr + 4, sizeof(ArcFeatureData));
    arc_ops.flash_buff_write((uint8_t*) &flag_end, arc_ops.save_addr + 4 + sizeof(ArcFeatureData), 4);
}

/*清除特征数据*/
void ClearArcFeature(void) {
    for (int i = 0; i < MAX_CHANNEL_NUM; i++) {
        for (int j = 0; j < MAX_FEATURE_STUDY_NUM; j++) {
            ArcFeatureData[i][j].Area[0] = 0;
            ArcFeatureData[i][j].Area[1] = 0;
            ArcFeatureData[i][j].Area[2] = 0;
            ArcFeatureData[i][j].effCurrent = 0;
        }
    }
    SaveArcFeature();
}

//特征是否有效
static bool IsEffectiveFeature(pArcFeature_TypeDef pArcFeature) {
    if (pArcFeature->Area[0] == 0 && pArcFeature->Area[1] == 0 && pArcFeature->Area[2] == 0
            && pArcFeature->effCurrent == 0) {
        return false;
    }

    return true;
}

/*增加学习特征并保存
 channel:通道号
 pArcFeature:特征参数
 */
static void AddArcFeature(int channel, pArcFeature_TypeDef pArcFeature) {
    memcpy(&ArcFeatureData[channel][0], &ArcFeatureData[channel][1],
    MAX_FEATURE_STUDY_NUM * sizeof(ArcFeature_TypeDef));

    ArcFeatureData[channel][MAX_FEATURE_STUDY_NUM - 1].Area[0] = pArcFeature->Area[0];
    ArcFeatureData[channel][MAX_FEATURE_STUDY_NUM - 1].Area[1] = pArcFeature->Area[1];
    ArcFeatureData[channel][MAX_FEATURE_STUDY_NUM - 1].Area[2] = pArcFeature->Area[2];
    ArcFeatureData[channel][MAX_FEATURE_STUDY_NUM - 1].effCurrent = pArcFeature->effCurrent;

    SaveArcFeature();
}

/*缓存特征数据
 channel:通道号
 pArcFeature:特征参数
 */
static void AddCacheFeature(int channel, pArcFeature_TypeDef pArcFeature) {
    memcpy(&ArcCacheData[channel][0], &ArcCacheData[channel][1],
    MAX_FEATURE_CACHE_NUM * sizeof(ArcFeature_TypeDef));

    ArcCacheData[channel][MAX_FEATURE_CACHE_NUM - 1].Area[0] = pArcFeature->Area[0];
    ArcCacheData[channel][MAX_FEATURE_CACHE_NUM - 1].Area[1] = pArcFeature->Area[1];
    ArcCacheData[channel][MAX_FEATURE_CACHE_NUM - 1].Area[2] = pArcFeature->Area[2];
    ArcCacheData[channel][MAX_FEATURE_CACHE_NUM - 1].effCurrent = pArcFeature->effCurrent;
}

/*计算面积特征
 current:电流buff
 zeroPoint:电压零点位置
 effCurrent:有效电流
 pArcFeature:特征参数
 */
static void CalArcFeature(float *current, int zeroPoint, float effCurrent, pArcFeature_TypeDef pArcFeature) {
    int offset = zeroPoint;
    float sum = 0;

    for (int i = 0; i < 3; i++) {
        sum = 0;
        for (int j = 0; j < 21; j++) {
            sum += fabs(current[offset]);
            offset++;
            if (offset >= 128) {
                offset = 0;
            }
        }
        pArcFeature->Area[i] = sum / effCurrent;
    }
    pArcFeature->effCurrent = effCurrent;
}

/*计算相似度*/
static float CalSimilarity(int channel, pArcFeature_TypeDef pArcFeature) {
    float maxNum = 100.0;
    float num = 0;
    float feature1[8] = { 0 };
    float feature2[8] = { 0 };

    for (int i = 0; i < MAX_FEATURE_STUDY_NUM; i++) {
        /*特征有效*/
        if (IsEffectiveFeature(&ArcFeatureData[channel][i]) == true) {
            feature1[0] = pArcFeature->Area[0];
            feature1[1] = pArcFeature->Area[1];
            feature1[2] = pArcFeature->Area[2];
            feature1[3] = pArcFeature->effCurrent;

            feature2[0] = ArcFeatureData[channel][i].Area[0];
            feature2[1] = ArcFeatureData[channel][i].Area[1];
            feature2[2] = ArcFeatureData[channel][i].Area[2];
            feature2[3] = ArcFeatureData[channel][i].effCurrent;

            num = getEuclideanDis(feature1, feature2, 4);
            if (num < maxNum) {
                maxNum = num;
            }
        }
    }

    return maxNum;
}

/*特征计算
 channel:通道号 0开始
 current:电流buff
 voltage:电压buff
 length:默认128
 effCurrent:有效电流
 SecondArcNum:1S电弧个数
 thisPeriodNum:当前波形电弧个数
 */
int ArcCalFeature(int channel, float *current, int zeroPoint, const int length, float effCurrent,
        int SecondArcNum, int thisPeriodNum) {
    static int lastArcNum[MAX_CHANNEL_NUM] = { 0 };
    ArcFeature_TypeDef arcFeature = { 0 };

    if (length != 128 || current == NULL || effCurrent == 0 || channel >= MAX_CHANNEL_NUM) {
#if LOG_ON == 1
        printf("para error!\r\n");
#endif
        return -1;
    }

    //电弧个数超过8个 && 正向增加  开始计算特征
    if (SecondArcNum >= 8 && (SecondArcNum - lastArcNum[channel]) >= 1 && thisPeriodNum > 0) {
        /*1、电压零点检测*/
        if (zeroPoint < 0 || zeroPoint >= length) {
#if LOG_ON == 1
            printf("zeroPoint error!\r\n");
#endif
            return -1;
        }
        /*2、计算电流特征*/
        CalArcFeature(current, zeroPoint, effCurrent, &arcFeature);

        if (arcFeature.Area[0] < 2 && arcFeature.Area[1] < 2 && arcFeature.Area[2] < 2) //面积过小,电弧可能已结束
                {
#if LOG_ON == 1
            printf("area too small!\r\n");
#endif
            return 0;
        }
        /*3、缓存特征*/
        AddCacheFeature(channel, &arcFeature);
#if LOG_ON == 1
        printf("\r\nArea1: %.1f\r\nArea2: %.1f\r\nArea3: %.1f\r\nIa:%.1f\r\n",
            arcFeature.Area[0],arcFeature.Area[1],arcFeature.Area[2],effCurrent);
#endif
    }

    lastArcNum[channel] = SecondArcNum;

    return 0;
}

/*电弧学习
 channel:通道号 0开始
 mode:模式 0:正常 1:学习模式
 */
int ArcStudyAnalysis(int channel, int mode) {
    float Possible[MAX_FEATURE_CACHE_NUM] = { 0 };
    int ArcLearnCnt = 0;

    if (channel >= MAX_CHANNEL_NUM) {
#if LOG_ON == 1
        printf("para error!\r\n");
#endif
        return -1;
    }

    //缓存特征相似度计算
    for (int i = 0; i < MAX_FEATURE_CACHE_NUM; i++) {
        if (IsEffectiveFeature(&ArcCacheData[channel][i]) == false) {
#if LOG_ON == 1
            printf("ArcCacheData error!\r\n");
#endif
            return -1;
        }

        Possible[i] = CalSimilarity(channel, &ArcCacheData[channel][i]);

        if (Possible[i] < 3) {
            ArcLearnCnt++;
        }
    }

#if LOG_ON == 1
    printf("\r\nPossible1: %.1f\r\nPossible2: %.1f\r\nPossible3: %.1f\r\n",
        Possible[0],Possible[1],Possible[2]);
#endif

    //超过半数已学习,不报警
    if (ArcLearnCnt >= (MAX_FEATURE_CACHE_NUM + 1) / 2) {
#if LOG_ON == 1
        printf("arc is learned!!\r\n");
#endif
        return 0;
    } else if (mode == 1) {
        for (int i = 0; i < MAX_FEATURE_CACHE_NUM; i++) {
            if (Possible[i] >= 3) {
#if LOG_ON == 1
                printf("study new feature\r\n");
#endif
                AddArcFeature(channel, &ArcCacheData[channel][i]);
                break;
            }
        }
        return 0;
    }

#if LOG_ON == 1
    printf("arc alarm!!\r\n");
#endif
    return 1;
}
