/***************************************************************************************
 ****************************************************************************************
 * FILE     : arcStudy.h
 * Description  :
 *
 * Copyright (c) 2021 by Tpson. All Rights Reserved.
 *
 * History:
 * Version      Name            Date            Description
 0.1       HuangDong        2021/01/14      Initial Version

 ****************************************************************************************
 ****************************************************************************************/

#ifndef __ARCFAULT_SMARTMODE_H_
#define __ARCFAULT_SMARTMODE_H_

#include "type_utils.h"

struct flash_operations {
    u32 save_addr; //特征保存地址
    //flash擦除接口
    void (*flash_erase)(uint32_t SectorAddr);
    //flash写接口
    void (*flash_buff_write)(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
    //flash读接口
    void (*flash_buff_read)(uint8_t *pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
};

/*初始化注册
 ops:flash操作接口
 返回值
 -1:参数错误
 0:设备第一次初始化
 1:已初始过
 */
int ArcfaultFlashOpsRegister(struct flash_operations *ops);

/*清除特征参数*/
void ClearArcFeature(void);

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
        int SecondArcNum, int thisPeriodNum);

/*电弧学习
 channel:通道号 0开始
 mode:模式 0:正常 1:学习模式
 */
int ArcStudyAnalysis(int channel, int mode);

#endif /*__ARCFAULT_SMARTMODE_H_*/

