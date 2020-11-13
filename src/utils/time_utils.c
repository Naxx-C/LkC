#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/**
 * return returns the time as the number of seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC)
 */
int getCurTime(void) {
    time_t timep;
    struct tm *p;
    int t = time(&timep);
    p = gmtime(&timep);
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
