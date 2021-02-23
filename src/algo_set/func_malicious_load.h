#ifndef FUNC_MALICIOUS_LOAD_
#define FUNC_MALICIOUS_LOAD_
#include "algo_base_struct.h"
#include "time_utils.h"
#define MALI_LOAD_SENSITIVITY_LOW 0
#define MALI_LOAD_SENSITIVITY_MEDIUM 1
#define MALI_LOAD_SENSITIVITY_HIGH 2
void setMaliLoadAlarmSensitivity(int channel, int mode);
int getMaliLoadAlarmSensitivity(int channel);
int maliciousLoadDetect(int channel, float *fft, float pulseI, float deltaActivePower,
        float deltaReactivePower, float effU, float activePower, float reactivePower, WaveFeature *deltaWf,
        DateStruct *date, char *errMsg);
int initFuncMaliLoad(void);
#endif
