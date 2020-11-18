#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "time_utils.h"
/**
 * return returns the time as the number of seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)
 */
int getCurTime(void) {
    time_t timep;
    int t = time(&timep);
//    p = gmtime(&timep);
//    printf("%d-%d-%d %d:%d:%d\n",1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, 8 + p->tm_hour, p->tm_min, p->tm_sec); /*获取当前秒*/
    return t;
//    printf("%d\n", p->tm_sec); /*获取当前秒*/
//    printf("%d\n", p->tm_min); /*获取当前分*/
//    printf("%d\n", 8 + p->tm_hour);/*获取当前时,这里获取西方的时间,刚好相差八个小时*/
//    printf("%d\n", p->tm_mday);/*获取当前月份日数,范围是1-31*/
//    printf("%d\n", 1 + p->tm_mon);/*获取当前月份,范围是0-11,所以要加1*/
//    printf("%d\n", 1900 + p->etm_year);/*获取当前年份,从1900开始，所以要加1900*/
//    printf("%d\n", p->tm_yday); /*从今年1月1日算起至今的天数，范围为0-365*/
}

//time函数在stm32平台用不了
int getDate(int *year, int *month, int *day) {
    time_t timep;
    struct tm *p;
    int t = time(&timep);
    if (t == 0)
        return -1;

    p = localtime(&timep); //gmtime函数 stm32系统不支持
    if (p == NULL)
        return -1;
    if (year != NULL)
        *year = 1900 + p->tm_year;
    if (month != NULL)
        *month = 1 + p->tm_mon;
    if (day != NULL)
        *day = p->tm_mday;
//    printf("%d-%d-%d %d:%d:%d\n",1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec); /*获取当前秒*/
    return t;
}

int getDateByTimestamp(int timestamp, DateStruct *date) {

    if (date == NULL)
        return -1;
    time_t timep = timestamp;
    struct tm *t = localtime(&timep);
    if (t == NULL)
        return -1;
    date->year = t->tm_year + 1900;
    date->mon = t->tm_mon + 1;
    date->mday = t->tm_mday;
    date->hour = t->tm_hour;
    date->min = t->tm_min;
    date->sec = t->tm_sec;
    date->wday = t->tm_wday == 0 ? 7 : t->tm_wday;

    return 0;
//    printf("%d-%d-%d %d:%d:%d\n",1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec); /*获取当前秒*/
}

