#ifndef __LOG_UTILS
#define __LOG_UTILS

//模组外注册的打印函数
int (*outprintf)(const char*, ...);
void registerPrintf(int print(const char*, ...));
#endif
