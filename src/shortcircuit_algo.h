#ifndef __SHORTCIRCUIT_ALGO
#define __SHORTCIRCUIT_ALGO
#define __WINDOWS
char isShortCircuit(float current, float ct);
char isShortByVoltage(float *curVol, float *lastVol, short startIndex, short compareLen, float thresh);
#endif
