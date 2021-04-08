#ifndef ALGO_SET_INTERNAL_H_
#define ALGO_SET_INTERNAL_H_
//平台识别使用
typedef struct {
    float activePower;
    float current;
    float voltage;
    float deltaActivePower;
    float deltaCurrent;
    float iPulse;
    int startupTime;
    float fft1;
    float fft3;
    float fft5;
    float fft7;
    float fft9;
    int unixTime;
} NilmCloudFeature;

/**
 *
 * DEMO:
 *  NilmCloudFeature *nilmCloudFeature = NULL;
 *  getNilmCloudFeature(1, &nilmCloudFeature);//nilmCloudFeature返回为空则没有事件需要上传
 *
 */
void getNilmCloudFeature(int channel, NilmCloudFeature **nilmCloudFeature);

#endif /* ALGO_SET_ALGO_SET_INTERNAL_H_ */
