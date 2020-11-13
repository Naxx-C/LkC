/*
 * Author: TPSON
 * 结构定义
 */
#ifndef ALGO_BASE_STRUCT_H_
#define ALGO_BASE_STRUCT_H_

typedef struct {
    int flatNum; //平肩数目
    int extremeNum; //极值点数目
    int maxLeftNum; //极大/极小值点左右的非平肩点数
    int maxRightNum;
    int minLeftNum;
    int minRightNum;
    float maxDelta;//最大正跳变
    float maxValue;//最大值
} WaveFeature;

typedef struct {

    float activePower; // powerline's total power, adjusted by voltage
    float reactivePower;
    float totalPowerCost;
    int sampleTime; // in second
} __attribute__ ((packed)) PowerTrack;

#endif /* ALGO_BASE_STRUCT_H_ */
