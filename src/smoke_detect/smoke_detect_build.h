/*
 * Author: TPSON
 */

#ifndef SMOKE_DETECT_BUILD_H_
#define SMOKE_DETECT_BUILD_H_

#define FR_ENABLE 1 // 红前
#define FB_ENABLE 1 // 蓝前
#define BR_ENABLE 1 // 红后

#define FR_ONLY_MODE 1 // 红前单通道
#define FR_FB_MODE 2 // 红前+蓝前
#define FR_FB_BR_MODE 3 // 红前+红后+蓝前
#define FR_BR_MODE 4 // 红前+红后

#define HW_MODE FR_FB_MODE //迷宫模式

#define SMOKE_NORMAL (0x1)
#define SMOKE_BLACK (0x1<<2)
#define NON_SMOKE_BIG_PARTICLE (0x1<<3)

#define LOG_ON 1

#endif /* SMOKE_DETECT_BUILD_H_ */
