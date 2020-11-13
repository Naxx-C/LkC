#ifndef __DORM_CONVERTER_ALGO
#define __DORM_CONVERTER_ALGO
/**ATTENTION!!!
 * You must define APP_BUILD_DATE somewhere as following code in your project
 * */
// const char *APP_BUILD_DATE = __DATE__;
/**
 * deltaCur:电流采样序列
 * curStart:序列起始位置.要保证从起始位置后至少有128个采样点
 * deltaVol:电压采样序列
 * volStart:序列起始位置.要保证从起始位置后至少有128个采样点
 * fft:谐波数组,大小为5,包含一三五七九次谐波分量
 * pulseI:启动冲量
 * deltaEffI:差分有效电流
 * deltaEffU:差分时有效电压(等于当前电压)
 * deltaActivePower:差分有功功率
 * errMsg:错误信息输出,如不需要可以置NULL
 */
typedef struct {

    float activePower; // powerline's total power, adjusted by voltage
    float reactivePower;
} __attribute__ ((packed)) PowerShaft;

typedef struct {
    int flatNum; //平肩数目
    int extremeNum; //极值点数目
    int maxLeftNum; //极值点左右的点数
    int maxRightNum;
    int minLeftNum;
    int minRightNum;
    float maxDelta;
    float maxValue;
} DormWaveFeature;
int isDormConverter(float *deltaCur, int curStart, float *deltaVol, int volStart, float deltaEffI,
        float deltaEffU, float deltaActivePower, char *errMsg);

#define DORM_CONVERTER_DISABLED -1
#define DORM_CONVERTER_SENSITIVITY_LOW 0
#define DORM_CONVERTER_SENSITIVITY_MEDIUM 1
#define DORM_CONVERTER_SENSITIVITY_HIGH 2
void setDormConverterMode(int mode);
int getDormConverterMode(void);
int getDormConverterAlgoVersion(void);
int getDormWaveFeature(float *cur, int curStart, float *vol, int volStart, float effI, float effU,
        float deltaActivePower, DormWaveFeature *cwf);
//init ok: return 0
int dormConverterAlgoInit(void);
#endif
