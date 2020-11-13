/*
 * Author: TPSON
 * 对外调用接口
 */
#ifndef ALGO_SET_H_
#define ALGO_SET_H_
/*
 * Feed original current and voltage sampling data to the algo modual
 * return error num. 0 is ok.
 * cur: realtime current sampling with 6.4k
 * curStart: start index of sample array
 * vol: realtime current sampling with 6.4k
 * volStart: start index of sample array
 * date: date in s, like 1605098030 (2020/11/11 20:33:50)
 * extraMsg: some info like error msg
 */
int feedData(float *cur, int curStart, float *vol, int volStart, int date, char *extraMsg);
int initTpsonAlgoLib();

void setMinEventDeltaPower(float minEventDeltaPower);

#endif /* ALGO_SET_ALGO_SET_H_ */
