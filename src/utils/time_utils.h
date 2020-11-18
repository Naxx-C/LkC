#ifndef __TIME_UTILS
#define __TIME_UTILS
typedef struct {
    int year;
    int mon;
    int mday;
    int hour;
    int min;
    int sec;
    int wday;
} DateStruct;
int getCurTime(void);
int getDate(int *year, int *month, int *day);
int getDateByTimestamp(int timestamp, DateStruct* curDate);
#endif
