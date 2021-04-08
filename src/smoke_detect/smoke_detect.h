/*
 * Author: TPSON
 */
#ifndef SMOKE_DETECT_H_
#define SMOKE_DETECT__H_
#include "type_utils.h"
#pragma pack(1)
typedef struct {
    U16 frontRed;
    U16 frontBlue;
    U16 backRed;
    U16 backgroundValue;
} SAMPLE_VALUE;

//烟雾颗粒类型
typedef enum {
    SMOKE_NORMAL, //常规烟雾
    SMOKE_BIG_PARTICLE, //大颗粒水蒸气灰尘等
    SMOKE_BLACK //黑烟
} SMOKE_TYPE;

//烟雾探测器故障类型
typedef enum {
    LIGHT_LEAK, //漏光
    MAZE_FAULT //迷宫污染
} SMOKE_DETECTOR_FAULT_TYPE;

#pragma pack(1)
typedef struct {
    U16 smokeTypeBitmap; //bitmap,0代表没有烟雾,其他烟雾类型见SOMKE_TYPE
    U8 faultTypeBitmap;
} SMOKE_DETECT_RESULT;

char smoke_detect(SMOKE_DETECT_RESULT *smokeDetectResult, SAMPLE_VALUE sampleValue, int unixTime);

#endif /* SMOKE_DETECT_H_ */
