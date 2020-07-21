#include "nilm_feature.h"
#include "nilm_appliance.h"

void addNormalizedFeature(NilmAppliance* na, float fv[]) {

    if (na->featureWriteIndex >= APP_FEATURE_NUM)
        na->featureWriteIndex = 0;
    NilmFeature* curFeature = &(na->nilmNormalizedFeatures[na->featureWriteIndex]);

    for (int i = 0; i < 5; i++) {
        curFeature->oddFft[i] = fv[i];
    }
    curFeature->powerFactor = normalizePowerFactor(fv[5]);
    curFeature->pulseI = normalizePulseI(fv[6]);
    curFeature->activePower = normalizeActivePower(fv[7]);
    na->featureWriteIndex++;
    if (na->featureCounter < APP_FEATURE_NUM) {
        na->featureCounter++;
    }
}
