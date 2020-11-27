#ifndef __NILM_FEATURE
#define __NILM_FEATURE

typedef struct {
    float oddFft[5];// 1/3/5/7/9 odd harmonic, 阻性负载多为x,0,0,0,0
    float powerFactor;// generally: [0.3,1], 阻性负载多为1
    float pulseI;// generally: [1,10], 阻性负载多为1
    float activePower; // generally: [300,3000]
} NilmFeature;

void copyNilmFeature(NilmFeature *dst, NilmFeature *src);

/**
 * 标准化 原因：不同特征的量纲差距比较大；部分特征实际是不稳定的，两次结果可能大不相同，
 *
 * @param feature
 * @param normalized
 */
float normalizePowerFactor(float powerFactor);

// 不稳定特征，分段化
float pulseILevel(float pulseI) ;

float normalizePulseI(float pulseI);

float normalizeActivePower(float activePower);
#endif
