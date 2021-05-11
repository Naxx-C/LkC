/*
 * Author: TPSON
 * 结构定义
 */
#ifndef ALGO_BASE_STRUCT_H_
#define ALGO_BASE_STRUCT_H_
#define LF 0.0001f //little float. 小值,防止分母为0

typedef struct {
    int flatNum; //平肩数目
    int extremeNum; //极值点数目
    int maxLeftNum; //32-点 极大/极小值点左右的非平肩点数
    int maxRightNum;
    int minLeftNum;
    int minRightNum;
    int maxLeftPointNum; //128-点 极大/极小值点左右的非平肩点数
    int maxRightPointNum;
    int minLeftPointNum;
    int minRightPointNum;
    float maxDelta;//最大正跳变
    float maxValue;//最大值
    float minNegDelta;//负半周最大跳变
    float minNegValue; //负半周最大值
} WaveFeature;

#pragma pack(1)
typedef struct {

    float activePower; // powerline's total power, adjusted by voltage
    float reactivePower;
    float totalPowerCost;
    int sampleTime; // in second
} PowerTrack;
#pragma pack()

#endif /* ALGO_BASE_STRUCT_H_ */
