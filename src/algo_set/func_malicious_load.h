#ifndef FUNC_MALICIOUS_LOAD_
#define FUNC_MALICIOUS_LOAD_
int maliciousLoadDetect(float *fft, float pulseI, float deltaActivePower, float deltaReactivePower, float effU,
        float activePower, float reactivePower, DateStruct *date, char *errMsg);
float setMaliLoadMinPower(float minPower);
void setMaliLoadWhitelistMatchRatio(float minRatio, float maxRatio);
#endif
