#ifndef FUNC_DORM_CONVERTER_
#define FUNC_DORM_CONVERTER_
#include "algo_base_struct.h"

#define DORM_CONVERTER_SENSITIVITY_LOW 0
#define DORM_CONVERTER_SENSITIVITY_MEDIUM 1
#define DORM_CONVERTER_SENSITIVITY_HIGH 2
void setDormConverterAlarmSensitivity(int channel, int mode);
int getDormConverterAlarmSensitivity(int channel);
int dormConverterDetect(int channel, float deltaActivePower, float deltaReactivePower, WaveFeature *wf,
        char *errMsg);
int dormConverterAdjustingCheck(int channel, float activePower, float reactivePower, WaveFeature *wf,
        char *errMsg);
int initFuncDormConverter(void);
#endif
