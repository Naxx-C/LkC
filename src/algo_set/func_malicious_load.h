#ifndef FUNC_MALICIOUS_LOAD_
#define FUNC_MALICIOUS_LOAD_
#include "algo_base_struct.h"
#include "time_utils.h"
int maliciousLoadDetect(float *fft, float pulseI, float deltaActivePower, float deltaReactivePower,
        float effU, float activePower, float reactivePower, WaveFeature *deltaWf, DateStruct *date,
        char *errMsg);
#endif
