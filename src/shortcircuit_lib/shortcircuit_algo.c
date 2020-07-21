#include "shortcircuit_algo.h"

#ifdef __WINDOWS
#include <math.h>
#else
#include "arm_math.h"
#endif
/**
 * judge by current sample
 *
 *  current: current sample
 *  ct: current transformer's config value
 */
char isShortCircuit(float current, float ct) {

    if (ct < 50)
        return 0;

    float thresh = ct * 1.39; //about peak value(1.414) for the config ct
    static short counter = 0;

    if (current >= thresh)
        counter++;
    else
        counter = 0; //must be continuous
    if (counter >= 10) {
        return 1;
    }
    return 0;
}
//float cur[5]={100,150,200,500,300};
//    for(int i=0;i<5;i++){
//    printf("%d\n", isShortCircuit(cur[i], 50.0));
//    }

static const short POINTS_IN_PERIOD = 128;
/**
 * judge by voltage
 *
 *	curVol: voltage array in this period
 * 	lastVol: voltage array in last period
 * 	startIndex: compare from
 *	compareLen: length to compare
 * 	ct: current transformer's config value
 */
char isShortByVoltage(float *curVol, float *lastVol, short startIndex, short compareLen, float thresh) {

    if (curVol == 0 || lastVol == 0 || startIndex + compareLen > POINTS_IN_PERIOD)
        return 0;

    float curEffVol=0;
    float lastEffVol=0;

#ifdef __WINDOWS
    for(int i=startIndex;i<startIndex+compareLen;i++){
        curEffVol+=curVol[i]*curVol[i];
        lastEffVol+=lastVol[i]*lastVol[i];
    }
    curEffVol=sqrtf(curEffVol);
    lastEffVol = sqrtf(lastEffVol);
#else
    arm_rms_f32(curVol + startIndex, compareLen, &curEffVol);
    arm_rms_f32(lastVol + startIndex, compareLen, &lastEffVol);
#endif
    float deltaAbs=curEffVol - lastEffVol;
    deltaAbs = deltaAbs >= 0.0f ? deltaAbs : -deltaAbs;
    if (deltaAbs > lastEffVol * 0.1)
        return 1;
    return 0;
}
