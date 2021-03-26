/*
 * Author: TPSON
 */

#ifndef ALGO_SET_BUILD_H_
#define ALGO_SET_BUILD_H_

#define CHANNEL_NUM 4
#define DEBUG_ONLY 0
//#define ARM_MATH_CM4
#define LOG_ON 0
#define OUTLOG_ON 1//对外输出的打印
#define DISABLE_FFT 0//禁用FFT计算,兼容一些低性能平台
#define TMP_DEBUG 0

/**客户化配置*/
//拓强电表
#define TUOQIANG_DEBUG 0
#if TUOQIANG_DEBUG
#undef CHANNEL_NUM
#define CHANNEL_NUM 3
#undef DEBUG_ONLY
#define DEBUG_ONLY 1
#endif


#endif /* ALGO_SET_BUILD_H_ */
