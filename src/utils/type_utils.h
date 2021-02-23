
/***************************************************************************************
****************************************************************************************
* FILE     : type_utils.h
* Description  :
*
* Copyright (c) 2019 by Tpson. All Rights Reserved.
*
* History:
* Version      Name            Date            Description
   1.0        HuangDong        2019/09/25    Initial Version

****************************************************************************************
****************************************************************************************/

#ifndef __TYPE_UTILS_H__
#define __TYPE_UTILS_H__

#include <stdbool.h>
#include <stdint.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

//typedef enum {ERROR = 0, SUCCESS = !ERROR} ErrorStatus;

typedef enum
{
    FALSE,
    TRUE
}BOOL;

typedef int   int32;
typedef short int16;
typedef char  int8;

typedef unsigned long long uint64;
typedef unsigned int       uint32;
typedef unsigned short     uint16;
typedef unsigned char      uint8;

typedef unsigned int       u32;
typedef unsigned short     u16;
typedef unsigned char      u8;
typedef signed   char      s8;

typedef volatile uint32_t  vu32;
typedef volatile uint16_t vu16;
typedef volatile uint8_t  vu8;

typedef signed char                 S8;
typedef unsigned char               U8;
typedef signed short                S16;
typedef unsigned short              U16;
typedef signed long                 S32;
typedef unsigned long               U32;

#endif //__TYPE_UTILS_H__
