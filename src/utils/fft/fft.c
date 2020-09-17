/*
 * fft.c
 *
 *  Created on: 2020年9月16日
 *      Author: 96875
 */
#include "zx_fft.h"
#include "fft.h"
#define  SAMPLE_NODES              (128)
COMPLEX x[SAMPLE_NODES];

static void initData(float *f) {
    int i;

    for (i = 0; i < SAMPLE_NODES; i++) {
        x[i].real = f[i];
        x[i].imag = 0.0f;
    }
}

/*
 * data/output must have size 128
 */
void do_fft(float *data, float *output) {
    initData(data);
    fft(x, SAMPLE_NODES);
    for (int i = 0; i < SAMPLE_NODES; i++) {
        output[i] = sqrt(x[i].real * x[i].real + x[i].imag * x[i].imag) / (SAMPLE_NODES / 2);
        //转换成真实频率下的幅值  fHz/SAMPLE_NODES 例如6.4k采样，output[1]就是50hz的幅值
//        printf("%.5f %.5f %.5f\n", x[i].real, x[i].imag,sqrt(x[i].real * x[i].real + x[i].imag * x[i].imag) / (SAMPLE_NODES / 2));
    }
}
